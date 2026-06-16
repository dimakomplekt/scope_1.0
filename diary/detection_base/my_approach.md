
# Осциллограф 


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


2. Поступивший сигнал передаётся на анализ детектору событий - экстремумы, переход через ноль

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

    2.1. PEAK-детектор

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

    2.2. extremum детектор

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

Итоговый детектор

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

        if (rise > ctrl->k_threshold * sigma &&
            ctrl->min_confidence > ctrl->min_confidence_threshold)
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

    2.3. zero-cross детектор

    Поиск пиков - не поиск максимума и минимума.
