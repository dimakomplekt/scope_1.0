
# Осциллограф 

## Сигналы

<img src="1.bmp" alt="Сигналы 1">
<img src="2.bmp" alt="Сигналы 2">


## EMA

EMA — это среднее значение, которое обновляется по мере прихода новых данных, но “забывает” старые значения всё быстрее и быстрее со временем.

    EMA = EMA_prev + α * (x - EMA_prev)

    или

    EMA = (1 - α) * EMA_prev + α * x


Допустим:

    EMA = 10
    α = 0.1

Приходит сигнал:

    x = 20
    обновление:
    EMA = 10 + 0.1 * (20 - 10)
    EMA = 11
    что произошло?
    сигнал прыгнул на +10
    EMA сдвинулась только на +1

то есть: EMA “не верит сразу”, но медленно подстраивается Ключевая идея EMA = память с забывание. Чем дальше прошлое — тем меньше его вес


Если разложить EMA по времени:

    x₀ влияет сильно
    x₁ влияет меньше
    x₂ ещё меньше

    α маленькое (например 0.01)
    очень плавная реакция
    почти “инерция”
    хорошо для DC-offset

    α большое (например 0.3)
    быстро реагирует
    чувствителен к шуму
    хорошо для детекторов событий


Почему EMA вообще используют в осциллографе:

    сглаживание шума
    отсутствие необходимости хранить буфер
    O(1) вычисления на sample

___

## Приём RAW значений

1. Осциллограф при подключении сигнала всегда принимает каждый такт программы какое-то value и time его приёма, обновляя основной голый буффер RAW_BUFFER = [value, time]. Буфер хранит историю сигнала без изменений (истинная “реальность” системы).

___

## Первая зона обработки сигнала

На каждом новом sample параллельно запускается вычисление потоковой модели сигнала.

Важно: это не фильтрация сигнала, а построение его текущего состояния.

___

2. Поступивший сигнал дублируется с работающим на каждом такте калькулятором running-значений на базе EMA - СИСТЕМЫ ДОВЕРИЯ к значениям и поиска уровня шумов SIGMA. Вычисления производятся инкрементально на базе лишь текущего head - 1 буффера


    1) running mean - mean ≈ EMA(x)
    2) running median - median ≈ sliding window / approximate update
    3) running DC-offset - dc ≈ blend(mean, median)
    4) running sigma - sigma ≈ EMA((x - dc)^2)


Система — это набор аккумуляторов:

    running_dc
    running_mean
    running_median_state
    running_sigma
    previous_x
    state_machine

___

    1.1. Running mean (EMA)

    что делаем на каждом sample:

        mean = mean + α * (x - mean);

смысл: “примерный центр сигнала”

    реагирует быстро
    чувствителен к изменениям
    хранит только одно число

___

    1.2. Running median (аппроксимация)

        Нет полной сортировки!

        потоковый вариант:

            if (x > median) median += α;
            else median -= α;


            или sliding window внутри малого подбуфера, заполняемого последними значениями из основного (например 32-64 сегмента данных)

смысл:

    устойчивый центр
    не боится выбросов

___

    1.3 DC-offset (рабочий центр)

        dc = blend(mean, median);

        Например:

            dc = 0.7 * median + 0.3 * mean;

Смысл: это “ноль системы”, вокруг которого всё живёт

___

    1.4 Sigma (шумовая модель) - оценка того, насколько сильно сигнал “размазан” вокруг центра (dc) (средний квадрат отклонения от центра)

        β — это скорость обучения sigma
        diff - текущее измерение “шума”

            diff = x - dc;
            sigma = sigma + β * (diff*diff - sigma);


Cмысл: измеряет разброс, показывает “насколько сигнал нестабилен”


    если β маленькое (например 0.01)

        σ меняется медленно
        система “спокойная”
        игнорирует резкие всплески

    если β большое (например 0.3)

        σ быстро подстраивается
        система “нервная”
        быстро реагирует на изменения сигнала

___

Используются только для принятия решений:

    где ноль
    где шум
    где событие
    где пик
    где переход состояния

___

### Примерный цикл прохода 1 от buffer[head - 1]:

```cpp

// вход: новый sample
void scope_buffer_analysis_1(Scope* used_scope)
{
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    if (buffer->count < 2) return;

    // =========================================================
    // 0. ДОСТАЁМ СИГНАЛ
    // =========================================================

    int curr_idx = buffer->head - 1;
    if (curr_idx < 0) curr_idx += BUFFER_SIZE;

    int prev_idx = buffer->head - 2;
    if (prev_idx < 0) prev_idx += BUFFER_SIZE;

    sample_t curr = buffer->samples[curr_idx];
    sample_t prev = buffer->samples[prev_idx];

    float x = curr.value;
    double t = curr.time;

    float x_prev = prev.value;

    // =========================================================
    // 1. STATE
    // =========================================================

    scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;

    float* running_mean   = &ctrl->running_mean;
    float* running_median = &ctrl->running_median;
    float* running_dc     = &ctrl->running_dc;

    float* sigma_squad = &ctrl->sigma_squad; // variance


    // =========================================================
    // 2. CENTER MODEL (running_mean / running_median / running_dc)
    // =========================================================

    // 2.1 running_mean (EMA)
    *running_mean += ctrl->alpha_mean * (x - *running_mean);

    // 2.2 running_median (robust drift)
    if (x > *running_median) *running_median += ctrl->alpha_median;
    else *running_median -= ctrl->alpha_median;

    // 2.3 running_dc (fusion center)
    *running_dc = 0.7f * (*running_median) + 0.3f * (*running_mean);


    // =========================================================
    // 3. NOISE MODEL (sigma^2)
    // =========================================================

    float diff = x - (*running_dc);
    float diff_squad = diff * diff;

    *sigma_squad += ctrl->beta_sigma * (diff_squad - *sigma_squad);

    float sigma = sqrtf(*sigma_squad);

    // =========================================================
    // 4. DYNAMIC THRESHOLD (zone)
    // =========================================================

    ctrl->k_threshold = ctrl->k_threshold * sigma;
}
```

___


3. Поступивший сигнал передаётся на анализ детектору событий - экстремумы, переход через ноль

___


Мы имеем сейчас всё, чтобы грамотно строить на своё усмотрение:

   1) amplitude_detector 
   2) zero_cross_detector 

это почти одинаковые задачи которые нужны для одного и того же: 

   1) либо найти период а потом найти экстремумы 
   2) либо найти экстремумы и потом найти период 
   
Есть правда такая тема, что сигнал может быть апериодичным соответственно в любом случае нам надо искать экстремумы ещё и отдельно

После блока 1–4 ты имеешь:

    running_dc        → центр сигнала
    sigma             → шум
    threshold         → зона неопределённости

Это означает:

    ты уже знаешь “где шум”, “где центр”, “насколько можно доверять отклонениям”

___

    3.1. PEAK-детектор

    Поиск пиков - не поиск максимума и минимума.

Ты НЕ ищешь: локальный максимум в окне
Ты ищешь: точку, где сигнал перестаёт расти и начинает падать (или наоборот)

Для детектора пиков имеются

```cpp

int trend;
int prev_trend;

float peak_candidate;
float trough_candidate;

float last_peak;
float last_trough;

```

Логика тренда

```cpp

if (x > x_prev) trend = RISING;

else if (x < x_prev) trend = FALLING;

```

Накопление кандидатов:

```cpp

if (trend == RISING) if (x > peak_candidate) peak_candidate = x;

if (trend == FALLING) if (x < trough_candidate) rough_candidate = x;

```

Фиксация пика:

```cpp

if (trend != prev_trend)
{
    if (prev_trend == RISING)
    {
        // был рост → теперь падение → это пик
        last_peak = peak_candidate;
    }

    if (prev_trend == FALLING)
    {
        // было падение → теперь рост → это впадина
        last_trough = trough_candidate;
    }

    // сброс кандидатов
    peak_candidate = x;
    trough_candidate = x;

    prev_trend = trend;
}

```

Применение трешхолда и сигмы!

```cpp

if (abs(x - running_dc) > threshold)

peak_valid = (peak - running_dc) > k * sigma;

```

Для анализа пиков и других точек добавляяются

```cpp

float trend_confidence;
float last_event_confidence;

```


Используются, как

``` cpp

if (x > x_prev)
    ctrl->trend_confidence += 1;
else
    ctrl->trend_confidence -= 1;

ctrl->trend_confidence *= (1.0f - sigma_norm);


float strength = (x - ctrl->running_dc) / (ctrl->sigma + 1e-6f);


if (ctrl->trend == FALLING &&
    ctrl->trend_confidence > ctrl->min_confidence &&
    strength > ctrl->k_threshold)
{
    ctrl->last_peak = ctrl->peak_candidate;
}

```

Итоговый детектор

``` cpp
void scope_buffer_analysis_2(Scope* used_scope)
{
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    if (buffer->count < 2) return;

    // =========================================================
    // 0. RAW SIGNAL
    // =========================================================

    int curr_idx = buffer->head - 1;
    if (curr_idx < 0) curr_idx += BUFFER_SIZE;

    int prev_idx = buffer->head - 2;
    if (prev_idx < 0) prev_idx += BUFFER_SIZE;

    sample_t curr = buffer->samples[curr_idx];
    sample_t prev = buffer->samples[prev_idx];

    float x = curr.value;
    double t = curr.time;

    float x_prev = prev.value;

    // =========================================================
    // 1. STATE
    // =========================================================

    scope_signal_control_ctx* ctrl =
        &used_scope->signal_control_data;

    // =========================================================
    // 2. MODEL (mean / median / dc / sigma)
    // =========================================================

    ctrl->running_mean += ctrl->alpha_mean * (x - ctrl->running_mean);

    if (x > ctrl->running_median)
        ctrl->running_median += ctrl->alpha_median;
    else
        ctrl->running_median -= ctrl->alpha_median;

    ctrl->running_dc =
        0.7f * ctrl->running_median +
        0.3f * ctrl->running_mean;

    float diff = x - ctrl->running_dc;
    float diff2 = diff * diff;

    ctrl->sigma_squad += ctrl->beta_sigma * (diff2 - ctrl->sigma_squad);

    float sigma = sqrtf(ctrl->sigma_squad);
    float threshold = ctrl->k_threshold * sigma;

    // =========================================================
    // 3. TREND DETECTION
    // =========================================================

    if (x > x_prev)
        ctrl->trend = 1;
    else if (x < x_prev)
        ctrl->trend = -1;

    // =========================================================
    // 4. PEAK / TROUGH CANDIDATES (noise-gated)
    // =========================================================

    float deviation = fabsf(x - ctrl->running_dc);

    // 0.1σ → почти гарантированный шум
    // 0.5σ → слабый сигнал, но уже интересный
    // 1.0σ → вероятно реальное отклонение
    // 2–3σ → почти точно событие

    if (ctrl->trend == 1)
    {
        if (deviation > 0.5f * sigma)
        {
            if (x > ctrl->peak_candidate) ctrl->peak_candidate = x;
        }
    }
    else if (ctrl->trend == -1)
    {
        if (deviation > 0.5f * sigma)
        {
            if (x < ctrl->trough_candidate) ctrl->trough_candidate = x;
        }
    }

    // =========================================================
    // 5. TREND CONFIDENCE (smoothed belief)
    // =========================================================

    float sigma_norm = sigma / (fabsf(ctrl->running_dc) + 1e-6f);

    // Если вверх подряд → confidence растёт
    // Если туда-сюда → он колеблется около нуля
    // Если хаос → он разрушается

    if (x > x_prev)
        ctrl->trend_confidence += 1.0f;
    else
        ctrl->trend_confidence -= 1.0f;

    // exponential-like decay by noise
    // чем больше шум — тем быстрее забывается уверенность в тренде
    ctrl->trend_confidence *= expf(-sigma_norm);


    // =========================================================
    // 6. NORMALIZED STRENGTH
    // =========================================================
    
    // 1e-6f - быстрая защита от деления на 0
    // сколько сигма-единиц текущая точка отклонена от центра

    // strength	    смысл
    // 0.0	        центр
    // 1.0	        слабое отклонение
    // 2.0	        заметный сигнал
    // 3.0+	        почти гарантированное событие
    // < 0	        ниже центра

    float strength = (x - ctrl->running_dc) / (sigma + 1e-6f);


    // =========================================================
    // 7. EVENT DETECTION (peak/trough commit)
    // =========================================================

    if (ctrl->trend != ctrl->prev_trend &&
        ctrl->trend_confidence > ctrl->min_confidence)
    {
        // -------------------------
        //          PEAK
        // -------------------------
        if (ctrl->prev_trend == 1)
        {
            if (strength > ctrl->k_threshold)
            {
                ctrl->last_peak = ctrl->peak_candidate;
                ctrl->last_peak_time = t;
                ctrl->last_peak_confidence = ctrl->trend_confidence;
            }
        }

        // -------------------------
        //          TROUGH
        // -------------------------
        if (ctrl->prev_trend == -1)
        {
            if (fabsf(strength) > ctrl->k_threshold)
            {
                ctrl->last_trough = ctrl->trough_candidate;
                ctrl->last_trough_time = t;
                ctrl->last_trough_confidence = ctrl->trend_confidence;
            }
        }

        // reset candidates
        ctrl->peak_candidate = x;
        ctrl->trough_candidate = x;

        ctrl->prev_trend = ctrl->trend;
    }
}

```

Мы собрали детектор, который нормально выдаёт нам восходящие и нисходящие пики (пики и  ямы очевидно) которым можно верить


___

    3.2. running-extremum детектор

Вводим

``` cpp

float max_candidate;
float min_candidate;

float max_confidence;
float min_confidence;

float max_velocity_score;
float min_velocity_score;

float last_confirmed_max;
float last_confirmed_min;

```

Чекаем

``` cpp

velocity = (x - prev_x)

smoothness = 1 / (|x - x_prev| + 1)

stability = sigma

confidence += smoothness / (sigma + 1e-6)
confidence -= abs(acceleration)

```

Максимум - “плавно росли → достигли → держались → потом чуть падение”:

    медленный рост
    высокая стабильность около вершины
    нет резкого скачка
    есть удержание значения

Плохой максимум:

    x резко прыгнул вверх
    нет предшествующего тренда
    быстро откатился назад


Обновление на примере максимума:

``` cpp

// Обновление
if (ctrl->trend == RISING)
{
    float slope = x - x_prev;

    float normalized = slope / (sigma + 1e-6f);

    if (normalized < ctrl->max_slope_limit)
    {
        ctrl->max_candidate = x;
        ctrl->max_confidence += 1.0f;
    }
}

// Скачки - штраф
if (fabsf(x - x_prev) > k * sigma)
{
    ctrl->max_confidence -= 2.0f;
}

// Удержание позиции
if (fabsf(x - ctrl->max_candidate) < sigma)
{
    ctrl->max_confidence += 0.5f;
}

// Фиксация
if (ctrl->trend == FALLING && ctrl->max_confidence > ctrl->min_confidence)
{
    ctrl->last_max = ctrl->max_candidate;
}

```

Минимум

Обновление кандидата минимума

``` cpp
// Обновление минимума
if (ctrl->trend == FALLING)
{
    float slope = x - x_prev;

    float normalized = slope / (sigma + 1e-6f);

    if (fabsf(normalized) < ctrl->min_slope_limit)
    {
        ctrl->min_candidate = x;
        ctrl->min_confidence += 1.0f;
    }
}

// Резкие провалы вниз — часто шум или выброс
if (fabsf(x - x_prev) > k * sigma)
{
    ctrl->min_confidence -= 2.0f;
}

// если сигнал держится около минимума → это валидная впадина
if (fabsf(x - ctrl->min_candidate) < sigma)
{
    ctrl->min_confidence += 0.5f;
}

// фиксация впадины при смене тренда
if (ctrl->trend == RISING &&
    ctrl->min_confidence > ctrl->min_confidence_threshold)
{
    ctrl->last_min = ctrl->min_candidate;
}

```

Итоговый детектор min-max

``` cpp
// 2.2. extremum детектор (финальный слой)
// цель: выбрать ДЕЙСТВИТЕЛЬНЫЕ max/min среди кандидатов из analysis_2
void scope_buffer_analysis_3(Scope* used_scope)
{
    scope_signal_control_ctx* ctrl =
    &used_scope->signal_control_data;

    scope_buffer_ctx* buffer =
    &ctrl->scope_buffer_data;

    if (buffer->count < 3) return;

    // =========================================================
    // 0. RAW SIGNAL (текущий и предыдущий)
    // =========================================================

    int curr_idx = buffer->head - 1;
    if (curr_idx < 0) curr_idx += BUFFER_SIZE;

    int prev_idx = buffer->head - 2;
    if (prev_idx < 0) prev_idx += BUFFER_SIZE;

    sample_t curr = buffer->samples[curr_idx];
    sample_t prev = buffer->samples[prev_idx];

    float x = curr.value;
    float x_prev = prev.value;

    float t = curr.time;

    // =========================================================
    // 1. NOISE SCALE (из первой стадии)
    // =========================================================

    float sigma = sqrtf(ctrl->sigma_squad);
    float inv_sigma = 1.0f / (sigma + 1e-6f);

    float velocity = x - x_prev;
    float abs_velocity = fabsf(velocity);

    // =========================================================
    // 2. MAX CANDIDATE UPDATE
    // =========================================================

    if (ctrl->trend == 1) // RISING
    {
        float smoothness = 1.0f / (abs_velocity + 1e-3f);
        float stability = inv_sigma;

        float acceleration_penalty = fabsf(
            velocity - ctrl->max_velocity_score
        );

        // накопление доверия к максимуму
        ctrl->max_confidence += smoothness * stability;
        ctrl->max_confidence -= acceleration_penalty;

        ctrl->max_velocity_score = velocity;

        // обновляем кандидата только если рост “чистый”
        if (velocity > 0 && smoothness * stability > 0.1f)
        {
            ctrl->max_candidate = x;
        }
    }

    // резкие выбросы вниз после роста → сигнал окончания max
    if (ctrl->trend == -1 && ctrl->prev_trend == 1)
    {
        float drop = ctrl->max_candidate - x;

        if (drop > ctrl->k_threshold * sigma &&
            ctrl->max_confidence > ctrl->min_confidence)
        {
            ctrl->last_confirmed_max = ctrl->max_candidate;
        }

        ctrl->max_confidence *= 0.5f; // частичный reset
    }

    // =========================================================
    // 3. MIN CANDIDATE UPDATE
    // =========================================================

    if (ctrl->trend == -1) // FALLING
    {
        float smoothness = 1.0f / (abs_velocity + 1e-3f);
        float stability = inv_sigma;

        float acceleration_penalty = fabsf(
            velocity - ctrl->min_velocity_score
        );

        ctrl->min_confidence += smoothness * stability;
        ctrl->min_confidence -= acceleration_penalty;

        ctrl->min_velocity_score = velocity;

        if (velocity < 0 && smoothness * stability > 0.1f)
        {
            ctrl->min_candidate = x;
        }
    }

    // резкий отскок вверх после падения → фикс min
    if (ctrl->trend == 1 && ctrl->prev_trend == -1)
    {
        float rise = x - ctrl->min_candidate;

        if (rise > ctrl->k_threshold * sigma && ctrl->min_confidence > ctrl->min_confidence_threshold)
        {
            ctrl->last_confirmed_min = ctrl->min_candidate;
        }

        ctrl->min_confidence *= 0.5f;
    }

    // =========================================================
    // 4. MEMORY UPDATE
    // =========================================================

    ctrl->prev_trend = ctrl->trend;
}

```

___

    3.3. zero-cross детектор + period детектор

Адекватное определение периода периодичного сигнала имеет различную сложность, в зависимости от вида рассматриваемого сигнала.

    Для периодичных сигналов возможны 3 уровня сложности детекции:

    1) Обычная волна
    2) Волна с составными волнами, отличающимися по времени
    3) Волна с составными волными, совпадающими по времени, но отличающимися по форме
   


<img src="3.bmp" alt="Период 1">


Первые два варианта могут детектироваться только по буфферу zero-cross с временными значениями

    Адекватно детектировать по буфферу zero-cross лишь с временными значениями 3 вид волны - невозможно. Поэтому, возможно, требуется фиксировать либо среднюю скорость изменения значений в каждой полуволне, либо и среднюю скорость изменения значений и площадь в каждой полуволне (сумму всех значений) и передавать её в дополнительный буффер.

В случае, если на руках будет скорость изменения значений, площадь полуволн и времена их крайних точек становится возможным определить паттерн даже сложных составных волн.

<img src="4.bmp" alt="Период 2">

    Возможно, с той же целью будет адекватнее делать какое-то быстрое Фурье для каждой мнимой волны и проч??? Так как 2 одинаковых спектральных характеристик для двух разных волн не бывает?

    Но полуволны содержат информацию, которую FFT теряет:

        временную структуру;
        порядок событий;
        асимметрию;
        форму фронтов;
        локальные особенности.


Общая структура определения периода, это работа: 

    Каждый пролет цикла: peak-detector + zero-cross-detector + halfwave-detector
    Редко: pattern-detector + period-counter


Zero-cross-детектор 

    Используется для поиска периода сигнала. Основная сложность заключается в том, что некоторые сигналы имеют период с несколькими разнонаправленными переходами через 0. Осциллограф с автоопределением периода обязан фиксировать паттерн сигнала и выводить на экран n-периодов сигнала.

    zero-cross детектор должен принимать дату о нисходящих и восходящих переходах сигнала через dc_offset - treshold и dc_offset + treshold, рассчитанных после EMA-фильтра (при этом - только сигналов в рамках доверия), фиксируя 2 временных метки перехода, по которым можно усредненно вычислить время настоящего перехода (без шумов). Передача значений в zero-cross-детектор может идти прямо из void scope_buffer_analysis_2(Scope* used_scope)

Halfwave-детектор

    Используется для анализа основных характеристик восходящих и падающих полуволн, которые будут использованы в паттерн-детекторе. 


Паттерн-детектор

    Задача паттерн-детектора - понять, между какими точками из буффера zero-cross имеется фиксированный паттерн высшего уровня. Фиксированный паттерн высшего уровня - полная волна (2 перехода в разную сторону снаружи по индексам которых нет других повторяющихся переходов, кроме подобных им), которая может содержать внутри себя другие - неполные волны (2 перехода в разную сторону снаружи по индексам которых есть другие повторяющиеся переходы).
    
Период 

    Период находится как среднее значение суммы времени всех паттернов высшего уровня в текущем буффере zero-cross


    Общая идея:

        1) Заапдейтить концовку void scope_buffer_analysis_2(Scope* used_scope) функционалом для отправки данных в zero-cross буффер на каждом шаге приёма сигнала
        2) Создать стейт-машину для грамотного приёма данных в буффер zero-cross на каждом шаге приёма сигнала
        3) С некоторой гораздо меньшей частотой запускать period_detector, включающий в себя pattern_analyser и определитель периода и частоты
        4) На базе данных о периоде обновлять данные о measured (не running!!!) экстремумах волны и средних / медианных значениях волны, путём просмотра 2-4 периодов (в зависимости от частоты - 2 для медленных разверток, 4 для быстрых) в текущем буффере через measured_extreme-детектор
        5) Обновить на базе measured значений текущие фильтры шума и показатели доверия 


    3.3.1 Zero-cross - детектор

Поскольку threshold задаётся симметрично относительно running_dc,
время перехода через истинный ноль оценивается как среднее между
моментами пересечения нижней и верхней границы зоны гистерезиса:

    zero_cross_time = (t_dc_minus_threshold + t_dc_plus_threshold) / 2

Такое усреднение дополнительно уменьшает влияние шума и дрожания
момента пересечения.

    Слой 1 — адаптация зоны
    threshold = f(sigma)

    Слой 2 — коррекция времени внутри зоны
    t_cross = (t_low + t_high) / 2

И вот вместе они дают устойчивость. Качество детекции = функция (threshold, sigma, модель времени фронта)


    Что считается завершённым переходом

        Например:

            Для RISING:

               1) пересек dc-th

               2) НЕ вернулся обратно

               3) пересек dc+th

               4) переход завершён

            Для FALLING:

                1) пересек dc+th

                2) НЕ вернулся обратно

                3) пересек dc-th

                4) переход завершён


    
Для реализации заявленного паттерн-детектора примеру, можно снабдить осциллограф буффером на базе цепи структур:


    ``` cpp

        // Очищенное время перехода, вычисленное на базе суммы t деленной на 2,
        // от переходов без смены курса, через dc_offset - treshold и dc_offset + treshold
        typedef struct cleaned_zero_cross
        {

            trend_type zero_cross_type;         // RISING (из - в +) или FALLING (из + в -)

            double time;                        // Время прохода

        } cleaned_zero_cross;


        // Данные о полуволне, по которым возможно провести анализ паттерна
        typedef struct halfwave_zero_crosses_ctx
        {
            // За 1 проход на приёме данных от buffer_former

            trend_type halfwave_type;                   // Характер полуволны

            double start_time;
            double end_time;

            // По принятым данным 
            double halfwave_full_time;                  // Примерное полное время полуволны
            

            // За тот же проход на приёме данных по главному буфферу :

            float peak_value;                           // Максимум (по главному буфферу от head - до head.halfwave_full_time)
            float trough_value;                         // Минимум (по главному буфферу от head - до head.halfwave_full_time)
            
            float halfwave_area;                        // Примерная площадь этой полуволны (по главному буфферу от head - до head.halfwave_full_time)
            float halfwave_average_speed;               // Примерная средняя скорость изменения значений в этой полуволне (по главному буфферу от head - до head.halfwave_full_time)

        } halfwave_zero_crosses_ctx;
            
        
        // Контекст анализатора переходов, который хранит данные
        // о переходах и выводит их обработку в буффер zero_crosses_detector с
        // определенным количеством точек cleaned_zero_cross, к примеру - 128 точками

        enum wave_pattern_buffer_former_states
        {
            STATE_WAIT_RISING_MINUS,
            STATE_WAIT_RISING_PLUS,

            STATE_WAIT_FALLING_PLUS,
            STATE_WAIT_FALLING_MINUS

        };


        // Двухпороговый захват времени с переобновлением текущей границы при повторных входах
        typedef struct wave_pattern_detector_buffer_former
        {

            /*

                я зашел в 1 зону, у меня rised_without_fall стоит false, сколько раз я бы не дропался и не возвращалолся,
                пока он false я просто обновляю rising_dc_minus_th_time на новое при новом заходе, как только я пересекаю 
                dc_offset + treshold, я ставлю rised_without_fall, как true, ставлю falled_without_rise, как false
                и curr_waited_type переключаю на FALLING

            */

            wave_pattern_buffer_former_states buffer_former_state;        // Переход какой точки RISING или FALLING через 0 ожидается на приход в формер

            halfwave_zero_crosses_ctx curr_halfwave;                    // Текущая анализируемая полуволна, которая при окончании анализа будет передана в zero_crosses_detector


            // Инкрементальный счётчик, демонстрирующий на сколько тиков мы ушли от head основного буффера сигнала в 
            // рамках текущей полуволны. 
            // 
            // При первичном оправдании ожидаемого в wave_pattern_buffer_former_states
            // стейта выставляет point_1_time и при посылке сигнала на отсутствие 
            // значимых переходов делает += 1. Если дальнейший значимый переход не оправдывает надежд
            // из wave_pattern_buffer_former_states - point_1_time сбрасывается (перезаписывается на следующем значимом переходе), а 
            // тип значимого перехода меняется на предыдущий по стейт машине. Если надежды
            // оправданы, то записывается point_2_time для текущего перехода, по point_1_time и point_2_time, как  
            // 0.5 (point_1_time + point_2_time) обновляется halfwave_zero_crosses[n] (в зависимости от типа перехода n = 0 или 1)

            double point_1_time;     // Время 1 перехода через ожидаемый dc_offset +- trashold
            double point_2_time;     // Время 2 перехода через ожидаемый dc_offset +- trashold

            cleaned_zero_cross halfwave_zero_crosses[2];        // Две заполняемые точки полуволны


            // При повторном входе в текущую границу (dc - threshold или dc + threshold)
            // время перехода перезаписывается, так как фиксируется
            // последняя точка устойчивого входа в зону гистерезиса

            // если начат переход
            // и тренд сменился

            // → отменить переход
            // → очистить временные метки
            // → ждать заново


            // В случае записи halfwave_zero_crosses[1] на этом же шаге внутри curr_halfwave по производимому рассчёту записываются:

                // trend_type halfwave_type;                   // Характер полуволны
                // double halfwave_full_time;                  // Полное время полуволны

                // На этом же шаге по текущему head основного буффера и значению halfwave_full_time производится рассчёт:

                // float halfwave_area;                        // Примерной площади этой полуволны
                // float halfwave_average_speed;               // Примерной средняя скорость изменения значений в этой полуволне


                // На этом же шаге по текущему head кольцевого zero_cross_detector буффера производится запись проанализированной полуволны в halfwaves_for_detection
                // текущего zero_crosses_detector:


        } wave_pattern_detector_buffer_former;


        // Хранит данные о последних 64 (128 / 2) вычищенных от шума полуволны с их показателями
        typedef struct wave_pattern_detector_ctx 
        {
            halfwave_zero_crosses_ctx halfwaves_for_detection[64];

            int head;
            int count;

        } wave_pattern_detector_ctx;


    ```
    
   Обозначенные действия должны производиться на максимальной скорости работы вычислительного устройства, после действий (из void scope_buffer_analysis_2(Scope* used_scope)): 

    ``` cpp
    // =========================================================
        // 7. EVENT DETECTION (peak/trough commit)
        // =========================================================

        if (ctrl->trend != ctrl->prev_trend &&
            ctrl->trend_confidence > ctrl->min_confidence)
        {
            // -------------------------
            //          PEAK
            // -------------------------
            if (ctrl->prev_trend == 1)
            {
                if (strength > ctrl->k_threshold)
                {
                    ctrl->last_peak = ctrl->peak_candidate;
                    ctrl->last_peak_time = t;
                    ctrl->last_peak_confidence = ctrl->trend_confidence;
                }
            }

            // -------------------------
            //          TROUGH
            // -------------------------
            if (ctrl->prev_trend == -1)
            {
                if (fabsf(strength) > ctrl->k_threshold)
                {
                    ctrl->last_trough = ctrl->trough_candidate;
                    ctrl->last_trough_time = t;
                    ctrl->last_trough_confidence = ctrl->trend_confidence;
                }
            }

            // reset candidates
            ctrl->peak_candidate = x;
            ctrl->trough_candidate = x;

            ctrl->prev_trend = ctrl->trend;

            // ВЗАИМОДЕЙСТВОВАТЬ С wave_pattern_detector_buffer_former ТУТ
        }

    ```


    3.3.2 period-детектор


    Далее, по текущему буфферу wave_pattern_detector_ctx становится возможно на более низкой скорости (к примеру - 240 Гц - в 4 раза чаще обновления дисплея) определять паттерн периода и его значение (также - значение частоты, а далее - значения амплитуд на периоде (не running, а просто по ограниченному куску основного буффера &used_scope->signal_control_data.scope_buffer_data))

    

    На каждом рассчётном шаге для 

    ```cpp

        void scope_period_detection(Scope* used_scope)
        {

            wave_pattern_detector_ctx wave_pattern_detector = &used_scope->signal_control_data.scope_wave_pattern_detector;

            // Понять по полуволнам из zero_cross_detector паттерн периода

            // Вычислить период

            // Вычислить частоту

            // Обновить значения measured_period и measured_frequency в памяти осциллографа

        }

    ```

    3.3.2.1 pattern-детектор


    3.3.2.2 F / T - вычислитель





    3.4. measured_extreme-детектор

    Далее, по найденному периоду возможно без особых затрат (для быстрых сигналов, для медленных - переиспользовать running значения) вычислить экстремумы и амплитуды, пользуясь участком буффера (head - 4T), примерно, как тут:


    ```cpp

    void scope_find_extreme(Scope* used_scope)
    {
        scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
        scope_buffer_ctx* buffer = &ctrl->scope_buffer_data;

        if (buffer->count < 4) return;
        if (ctrl->current_frequency_value <= 0.0f) return;

        int head = buffer->head;

        // =========================================================
        // 1. определяем окно ~ 3 периода сигнала
        // =========================================================

        double freq = (double)ctrl->current_frequency_value;

        int samples_per_period = (int)(SCOPE_SAMPLE_RATE / freq);
        int window_size = samples_per_period * 3;

        if (window_size < 10)
            window_size = 10;

        if (window_size > buffer->count)
            window_size = buffer->count;

        // =========================================================
        // 2. границы кольцевого окна
        // =========================================================

        int start = (head - window_size + BUFFER_SIZE) % BUFFER_SIZE;
        int end   = (head - 1 + BUFFER_SIZE) % BUFFER_SIZE;

        // =========================================================
        // 3. поиск min/max
        // =========================================================

        float min_value = FLT_MAX;
        float max_value = -FLT_MAX;

        int i = start;

        while (1)
        {
            float v = buffer->samples[i].value;

            if (v < min_value) min_value = v;
            if (v > max_value) max_value = v;

            if (i == end)
                break;

            i = (i + 1) % BUFFER_SIZE;
        }

        ctrl->current_min_signal_value = min_value;
        ctrl->current_max_signal_value = max_value;

        // =========================================================
        // 4. амплитуда
        // =========================================================

        float raw_amplitude = (max_value - min_value) * 0.5f;

        // первый запуск
        if (!ctrl->amplitude_initialized)
        {
            ctrl->amplitude_estimate = raw_amplitude;
            ctrl->amplitude_initialized = true;
        }
        else
        {
            // ОДНО сглаживание (важно: убрали двойное)
            ctrl->amplitude_estimate =
                0.85f * ctrl->amplitude_estimate +
                0.15f * raw_amplitude;
        }

        if (ctrl->amplitude_estimate < 0.001f)
            ctrl->amplitude_estimate = 0.001f;

        // =========================================================
        // 5. обновление threshold (не каждый вызов!)
        // =========================================================

        ctrl->threshold_update_accumulator++;

        const int THRESHOLD_UPDATE_RATE = 10;

        if (ctrl->threshold_update_accumulator >= THRESHOLD_UPDATE_RATE)
        {
            float zc_threshold = ctrl->amplitude_estimate * 0.02f;

            if (zc_threshold < 0.001f)
                zc_threshold = 0.001f;

            ctrl->controlled_signal->current_treshold = zc_threshold;

            ctrl->threshold_update_accumulator = 0;
        }
    }

    ```


### Обновление фильтра и доверия по найденным значениям

4. После завершения measuring-части прохода цикла, необходимо внести корректировки в параметры обратной связи...






## Общая Инфа
Главный компромисс: Скорость vs. Фазовый сдвиг (Lag)

Любая система на базе EMA (экспоненциального сглаживания) страдает от фундаментальной проблемы: чем сильнее мы хотим отфильтровать шум, тем сильнее мы опаздываем за реальным сигналом.Если сделать \(\alpha \) большим (быстрая реакция), система выдаст актуальные данные «прямо сейчас», но будет бешено реагировать на случайные ложные всплески (рыночные манипуляции или электромагнитные наводки).Если сделать \(\alpha \) маленьким (гладкий тренд/линия нуля), система станет стабильной, но «узнает» о смене тренда или пробитии уровня с опозданием (фазовым сдвигом). 

В трейдинге это опоздание стоит денег, в осциллографе — искажает форму высокочастотного фронта.Что придумали, чтобы сделать еще лучше?

Когда разработчикам осциллографов или HFT-роботов не хватает базового набора [Mean, Median, DC, Sigma], они используют модификации этих же потоковых принципов:

Адаптивный коэффициент (например, Кауфман / индикатор AMA):

Вместо константных \(\alpha \) и \(\beta \) их делают динамическими. Система сама измеряет «эффективность» движения через ту же Сигму. Если на рынке флэт (шум), \(\alpha \) автоматически падает до минимума (система спит). Если начинается мощный импульс, \(\alpha \) мгновенно взлетает до 1.0, и система переходит в режим «пропускать сигнал без фильтрации».

Фильтры Калмана (Kalman Filter):

Это шаг вперед по сравнению с EMA. Вместо простого сглаживания прошлого, фильтр Калмана строит физико-математическую модель предсказания (например: «если цена росла со скоростью X, то в следующий такт она должна быть Y») и сравнивает предсказание с реальностью, корректируя веса доверия на лету. Он тоже работает за \(O(1)\) и вычисляется инкрементально.

Потоковый Welford’s алгоритм: Для вычисления точной Sigma (а не аппроксимированной через EMA) используют алгоритм Велфорда. Он позволяет считать истинное математическое математическое ожидание и дисперсию строго в один проход, без накопления численной погрешности float (что часто бывает проблемой в diff * diff на длинных дистанциях).