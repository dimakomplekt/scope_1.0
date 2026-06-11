1. Роллинг (плывущий) — как в Mario
   
Уже описали — новые данные справа, всё смещается влево.

2. Статический с перерисовкой (то, что нужно)
   
Показываем последние N точек на полную ширину экрана. Экран НЕ сдвигается, а перерисовывается целиком каждый кадр.

``` cpp
typedef struct {
    // Буфер данных (кольцевой)
    float *samples;        // значения сигнала
    Uint32 *timestamps;    // временные метки (ms)
    int buffer_size;       // ёмкость буфера
    int write_index;       // куда писать новую точку
    int count;             // сколько точек накоплено
    
    // Параметры отображения
    float time_scale;      // секунд на всю ширину экрана
    float volt_scale;      // вольт на пиксель (или на высоту)
    int offset_y;          // смещение нуля по вертикали (пиксели)
    
    // Режим
    bool autoscale;        // авто-масштаб времени/амплитуды
} Oscilloscope;

// Добавление точки
void osc_add_point(Oscilloscope *osc, Uint32 time_ms, float value) {
    int idx = osc->write_index;
    osc->samples[idx] = value;
    osc->timestamps[idx] = time_ms;
    
    osc->write_index = (osc->write_index + 1) % osc->buffer_size;
    if (osc->count < osc->buffer_size) osc->count++;
}

// Отрисовка статического режима
void osc_draw_static(SDL_Renderer *renderer, Oscilloscope *osc, SDL_Rect display_rect) {
    if (osc->count < 2) return;
    
    // Определяем временной диапазон для показа
    Uint32 current_time = SDL_GetTicks(); // или ваш глобальный таймер
    Uint32 time_start = current_time - (Uint32)(osc->time_scale * 1000);
    Uint32 time_end = current_time;
    
    // Находим индексы для отображения
    int first_idx = -1, last_idx = -1;
    for (int i = 0; i < osc->count; i++) {
        int idx = (osc->write_index - 1 - i + osc->buffer_size) % osc->buffer_size;
        if (osc->timestamps[idx] >= time_start) first_idx = idx;
        if (osc->timestamps[idx] <= time_end) last_idx = idx;
        if (first_idx != -1 && last_idx != -1) break;
    }
    
    if (first_idx == -1) return;
    
    // Рисуем линии
    int prev_x = -1, prev_y = -1;
    for (int i = first_idx; i != last_idx; ) {
        int idx = i;
        float x_percent = (float)(osc->timestamps[idx] - time_start) / (osc->time_scale * 1000);
        int x = display_rect.x + (int)(x_percent * display_rect.w);
        
        float y_percent = 0.5f - (osc->samples[idx] / (osc->volt_scale * 2));
        int y = display_rect.y + (int)(y_percent * display_rect.h);
        
        if (prev_x != -1) {
            SDL_RenderDrawLine(renderer, prev_x, prev_y, x, y);
        }
        
        prev_x = x;
        prev_y = y;
        
        // Переход к следующей точке
        i = (i + 1) % osc->buffer_size;
        if (i == last_idx) break;
    }
}

``` 

3. Режим «несколько периодов» без сдвига


Для отображения фиксированного количества периодов нужно определять частоту сигнала:



``` cpp

typedef struct {
    // ... предыдущие поля +
    float detected_freq;   // обнаруженная частота (Гц)
    int periods_to_show;   // сколько периодов показывать (1, 2, 3...)
} Oscilloscope;

void osc_update_time_scale_by_freq(Oscilloscope *osc) {
    if (osc->detected_freq > 0) {
        // Время на экран = периоды / частота
        osc->time_scale = osc->periods_to_show / osc->detected_freq;
    }
}


``` 


4. Отрисовка сетки



``` cpp

void osc_draw_grid(SDL_Renderer *renderer, SDL_Rect display_rect, 
                   float time_scale, float volt_scale, int pixels_per_div) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    
    // Вертикальные линии (время)
    int num_h_divs = display_rect.w / pixels_per_div;
    for (int i = 0; i <= num_h_divs; i++) {
        int x = display_rect.x + i * pixels_per_div;
        SDL_RenderDrawLine(renderer, x, display_rect.y, 
                          x, display_rect.y + display_rect.h);
    }
    
    // Горизонтальные линии (амплитуда)
    int num_v_divs = display_rect.h / pixels_per_div;
    for (int i = 0; i <= num_v_divs; i++) {
        int y = display_rect.y + i * pixels_per_div;
        SDL_RenderDrawLine(renderer, display_rect.x, y,
                          display_rect.x + display_rect.w, y);
    }
    
    // Подписи (опционально, с помощью SDL_ttf)
}


```


5. Рекомендация по архитектуре


``` cpp

// В главном цикле
while (running) {
    Uint32 now = get_global_time();
    float value = sine_generator_get_value(now);
    
    osc_add_point(&osc, now, value);
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    osc_draw_grid(renderer, display_rect, osc.time_scale, osc.volt_scale, 50);
    osc_draw_static(renderer, &osc, display_rect);
    
    SDL_RenderPresent(renderer);
    SDL_Delay(16); // ~60 FPS
}



``` 

Ключевое отличие: в статическом режиме вы не храните очередь «время-значение» как линейный буфер для прокрутки, а вычисляете, какие точки попадают в текущее временное окно, и рисуете их абсолютные координаты. При добавлении новой точки просто перерисовываете весь экран — это даёт эффект «застывшего» сигнала, который обновляется справа налево без сдвига.

Для авто-масштабирования нужно отслеживать min/max значений в буфере и подстраивать volt_scale.


Режим "n периодов" — это НЕ усреднение
Это показ последних n полных периодов сигнала без сдвига, один под одним (как на реальном осциллографе в режиме TRIGGER).


Как это работает:

Определяем момент триггера (например, пересечение нуля снизу вверх)

Запоминаем точки одного периода от триггера до следующего триггера

Накладываем несколько периодов друг на друга при отрисовке

Новые данные накапливаются в буфер периодов, старые вытесняются

``` cpp

typedef struct {
    // Буфер для хранения нескольких периодов
    float **periods_buffer;   // [max_periods][samples_per_period]
    int current_period;       // индекс текущего записываемого периода
    int samples_in_period;     // сколько точек в текущем периоде
    
    int trigger_state;         // 0 - ждём, 1 - записываем
    float last_value;          // для детекта пересечения нуля
    
    int periods_to_show;       // 1, 2, 4...
    int max_periods;           // сколько храним в буфере
} ScopePeriodic;

void scope_add_sample(ScopePeriodic *s, float value) {
    // Детект триггера (пересечение 0 снизу вверх)
    if (s->trigger_state == 0 && s->last_value <= 0 && value > 0) {
        s->trigger_state = 1;               // начали запись периода
        s->samples_in_period = 0;
        
        // Переключаемся на следующий период в буфере
        s->current_period = (s->current_period + 1) % s->max_periods;
    }
    
    if (s->trigger_state == 1) {
        // Записываем точку в текущий период
        s->periods_buffer[s->current_period][s->samples_in_period++] = value;
        
        // Детект конца периода (новый триггер = конец старого)
        if (s->samples_in_period > 10 && s->last_value <= 0 && value > 0) {
            s->trigger_state = 0;   // период закончен
        }
    }
    
    s->last_value = value;
}

// Отрисовка: рисуем ВСЕ хранимые периоды поверх друг друга
void scope_draw(ScopePeriodic *s, SDL_Renderer *r, SDL_Rect rect) {
    for (int p = 0; p < s->periods_to_show && p < s->max_periods; p++) {
        int period_idx = (s->current_period - p + s->max_periods) % s->max_periods;
        
        // Рисуем линии для этого периода
        for (int i = 1; i < s->samples_in_period; i++) {
            int x1 = rect.x + (i-1) * rect.w / s->samples_in_period;
            int x2 = rect.x + i * rect.w / s->samples_in_period;
            
            float y1 = rect.y + rect.h/2 - s->periods_buffer[period_idx][i-1] * rect.h/2;
            float y2 = rect.y + rect.h/2 - s->periods_buffer[period_idx][i] * rect.h/2;
            
            SDL_RenderDrawLine(r, x1, y1, x2, y2);
        }
    }
}

```


Как делать показ периода если я только начал съем данных и даты где нет да и период не определился


Решение: 3 состояния осциллографа

``` cpp

typedef enum {
    SCOPE_WAITING_TRIGGER,  // ждём первый триггер, показываем "ждите"
    SCOPE_FIRST_PERIOD,     // собираем первый период (не показываем)
    SCOPE_NORMAL            // нормальная работа, показываем периоды
} ScopeState;

typedef struct {
    ScopeState state;
    int samples_collected;   // сколько точек собрали в текущем периоде
    float trigger_level;     // уровень триггера (например, 0.0)
    int pretrigger_samples;  // сколько точек ДО триггера показывать
    
    // Буфер для претриггера (кольцевой)
    float *pretrigger_buffer;
    int pretrigger_index;
} ScopeTrigger;

```

1. Минимальная — просто ждём с индикацией

``` cpp

void scope_update(ScopeTrigger *s, float value, Uint32 time) {
    switch(s->state) {
        case SCOPE_WAITING_TRIGGER:
            // Детектим пересечение уровня
            if (s->last_value <= s->trigger_level && value > s->trigger_level) {
                s->state = SCOPE_FIRST_PERIOD;
                s->samples_collected = 0;
            }
            break;
            
        case SCOPE_FIRST_PERIOD:
            // Собираем точки, не рисуем
            buffer_add(s->period_buffer, value);
            s->samples_collected++;
            
            // Нашли конец периода
            if (s->samples_collected > 10 && 
                s->last_value <= s->trigger_level && value > s->trigger_level) {
                s->state = SCOPE_NORMAL;
            }
            break;
            
        case SCOPE_NORMAL:
            // Нормальная работа с отрисовкой
            // ...
            break;
    }
}

// Отрисовка
void scope_draw(ScopeTrigger *s) {
    if (s->state == SCOPE_WAITING_TRIGGER) {
        // Рисуем "NO SIGNAL" или ожидание
        DrawText("Waiting for trigger...", display_rect.x + 10, display_rect.y + 10);
        DrawAnimatedDots();  // анимируем точки
    } 
    else if (s->state == SCOPE_FIRST_PERIOD) {
        DrawText("Capturing first period...", display_rect.x + 10, display_rect.y + 10);
        DrawProgressBar(s->samples_collected / expected_period_length);
    }
    else {
        DrawNormalWaveform();  // обычная отрисовка периодов
    }
}

```


С претриггером (pretrigger) — показываем ДО того как случился триггер


```cpp

void scope_init(ScopeTrigger *s, int max_samples, int pretrigger_count) {
    s->pretrigger_buffer = malloc(sizeof(float) * pretrigger_count);
    s->pretrigger_count = pretrigger_count;
    s->pretrigger_index = 0;
}

void scope_add_sample(ScopeTrigger *s, float value) {
    // Всегда пишем в претриггерный буфер (кольцевой)
    s->pretrigger_buffer[s->pretrigger_index] = value;
    s->pretrigger_index = (s->pretrigger_index + 1) % s->pretrigger_count;
    
    // Детект триггера
    if (s->state == SCOPE_WAITING_TRIGGER && 
        s->last_value <= s->trigger_level && value > s->trigger_level) {
        
        // Нашли триггер! Копируем претриггер в начало периода
        for (int i = 0; i < s->pretrigger_count; i++) {
            int idx = (s->pretrigger_index + i) % s->pretrigger_count;
            s->period_buffer[i] = s->pretrigger_buffer[idx];
        }
        s->samples_collected = s->pretrigger_count;
        s->state = SCOPE_FIRST_PERIOD;
    }
    
    if (s->state == SCOPE_FIRST_PERIOD) {
        s->period_buffer[s->samples_collected++] = value;
        // ... проверка конца периода
    }
    
    s->last_value = value;
}


```


Автоматический режим (Auto) — если триггера нет долго, показываем "как есть"

``` cpp

void scope_update_auto(ScopeTrigger *s, float value, Uint32 now) {
    static Uint32 last_trigger_time = 0;
    
    if (s->state == SCOPE_WAITING_TRIGGER) {
        if (now - last_trigger_time > 500) {  // 500ms без триггера
            // Переходим в авто-режим: рисуем всё подряд
            s->state = SCOPE_AUTO_ROLL;
            last_trigger_time = now;
        }
    }
    
    if (s->state == SCOPE_AUTO_ROLL) {
        // Рисуем роллинг (плывущий режим) как временное решение
        draw_rolling_waveform(value);
        
        // Продолжаем искать триггер
        if (s->last_value <= s->trigger_level && value > s->trigger_level) {
            s->state = SCOPE_FIRST_PERIOD;  // нашли! переключаемся обратно
            last_trigger_time = now;
        }
    }
}

```



Для определения периода:
Не надо ловить экстремумы! Они шумные и нестабильные. Используй переход через ноль:

``` cpp

// Простой и надёжный способ
float last_value = 0;
Uint32 last_cross_time = 0;
float period = 0;

void detect_period(float current_value, Uint32 current_time) {
    // Переход снизу вверх через 0
    if (last_value <= 0 && current_value > 0) {
        if (last_cross_time != 0) {
            period = (current_time - last_cross_time) / 1000.0; // в секундах
            frequency = 1.0 / period;
        }
        last_cross_time = current_time;
    }
    last_value = current_value;
}

```


АЦП (реальный или симуляция)
    ↓
[значение, время]  ← ты здесь
    ↓
Определение периода (переход через ноль)
    ↓
Накопление одного периода в буфер
    ↓
Отрисовка (без всяких Фурье!)
    ↓
Показ на экране


``` cpp

// Что ты ДЕЙСТВИТЕЛЬНО можешь делать с формой сигнала:

// 1. Просто рисовать как есть (99% задач)
draw_waveform(samples, count);

// 2. Нормализовать амплитуду (для автоподбора масштаба)
normalize_to_screen(samples, screen_height, zero_position);

// 3. Сглаживание (медианный фильтр для убивания шума)
median_filter(samples, filtered, window=3);

// 4. Интерполяция (если точек мало, а рисовать надо плавно)
draw_cubic_spline(samples, count);  // рисует гладкие кривые

// 5. Детект фронтов (для цифровых сигналов)
detect_rising_edge(samples, threshold=2.5);

```


Примеры функций обработки обновлений осциллографа 

```cpp

void osc_update(Oscilloscope *osc, Uint32 time_ms, float value) {
    // 1. Всегда сохраняем в сырой буфер (нужен для всех режимов)
    int idx = osc->write_idx;
    osc->samples[idx] = value;
    osc->timestamps[idx] = time_ms;
    osc->write_idx = (osc->write_idx + 1) % osc->buffer_size;
    if (osc->count < osc->buffer_size) osc->count++;
    
    // 2. Обновляем детекцию периода (нужна для PERIOD_BASED)
    detect_period(osc, value, time_ms);
    
    // 3. Если в режиме периодов - накапливаем текущий период
    if (osc->mode == MODE_PERIOD_BASED) {
        update_period_buffer(osc, value);
    }
    
    // 4. Помечаем, что рендер-кэш устарел
    osc->render_points_count = 0;
}

void detect_period(Oscilloscope *osc, float value, Uint32 time_ms) {
    // Переход через ноль снизу вверх
    if (osc->last_value <= osc->trigger_level && value > osc->trigger_level) {
        if (osc->last_trigger_time != 0) {
            float period_ms = time_ms - osc->last_trigger_time;
            osc->detected_freq = 1000.0f / period_ms;
        }
        osc->last_trigger_time = time_ms;
    }
    osc->last_value = value;
}

void update_period_buffer(Oscilloscope *osc, float value) {
    static int period_samples = 0;
    
    // Начало нового периода по триггеру
    if (osc->last_value <= osc->trigger_level && value > osc->trigger_level) {
        period_samples = 0;
    }
    
    if (period_samples < osc->buffer_size) {
        osc->current_period[period_samples++] = value;
    }
    
    // Когда период накоплен, сразу строим рендер-кэш
    if (period_samples > 10 && (osc->last_value <= osc->trigger_level && value > osc->trigger_level)) {
        build_period_render_cache(osc, period_samples);
        period_samples = 0;
    }
}

```


Render cash build


``` cpp

void build_render_cache(Oscilloscope *osc, SDL_Rect display_rect) {
    if (osc->mode == MODE_TIME_BASED) {
        if (osc->scroll_enabled)
            build_time_scroll_cache(osc, display_rect);
        else
            build_time_static_cache(osc, display_rect);
    } else {
        build_period_static_cache(osc, display_rect);
    }
}

// Режим 1a: Временная развёртка С уходом влево (роллинг)
void build_time_scroll_cache(Oscilloscope *osc, SDL_Rect rect) {
    int w = rect.w, h = rect.h;
    float time_window_ms = osc->time_per_div * 10.0f * 1000.0f; // 10 делений
    
    Uint32 now = SDL_GetTicks(); // текущее время
    Uint32 time_start = now - time_window_ms;
    
    // Идём от новых к старым (справа налево)
    int points = 0;
    for (int i = 0; i < osc->count; i++) {
        int idx = (osc->write_idx - 1 - i + osc->buffer_size) % osc->buffer_size;
        if (osc->timestamps[idx] < time_start) break;
        
        float x_percent = (osc->timestamps[idx] - time_start) / time_window_ms;
        osc->render_points[points].x = rect.x + (int)(x_percent * w);
        osc->render_points[points].y = rect.y + h/2 - (int)(osc->samples[idx] * h/4);
        points++;
    }
    osc->render_points_count = points;
}

// Режим 1b: Временная развёртка БЕЗ ухода (окно зафиксировано)
void build_time_static_cache(Oscilloscope *osc, SDL_Rect rect) {
    // ОКНО НЕ ДВИГАЕТСЯ: показываем последние N секунд
    float time_window_ms = osc->time_per_div * 10.0f * 1000.0f;
    Uint32 time_end = osc->timestamps[osc->write_idx - 1]; // последняя точка
    Uint32 time_start = time_end - time_window_ms;
    
    // Аналогично scroll, но окно привязано к последней точке
    // ... (тот же код, но time_start/end фиксированы)
}

// Режим 2: Периодная развёртка (n периодов без ухода)
void build_period_static_cache(Oscilloscope *osc, SDL_Rect rect) {
    if (osc->detected_freq <= 0) return;
    
    float period_ms = 1000.0f / osc->detected_freq;
    float window_ms = period_ms * osc->periods_to_show;
    
    // Находим точки, попадающие в последние N периодов
    Uint32 time_end = osc->timestamps[osc->write_idx - 1];
    Uint32 time_start = time_end - window_ms;
    
    // Строим точки аналогично time_static
    // ...
}

```



Render

``` cpp

void osc_render(Oscilloscope *osc, SDL_Renderer *renderer, SDL_Rect display_rect) {
    // 1. Рисуем сетку (всегда по текущим параметрам)
    draw_grid(renderer, display_rect, osc);
    
    // 2. Если кэш устарел или изменился режим — перестраиваем
    if (osc->render_points_count == 0) {
        build_render_cache(osc, display_rect);
    }
    
    // 3. Рисуем сигнал по точкам из кэша
    if (osc->render_points_count > 1) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        for (int i = 1; i < osc->render_points_count; i++) {
            SDL_RenderDrawLine(renderer,
                osc->render_points[i-1].x, osc->render_points[i-1].y,
                osc->render_points[i].x, osc->render_points[i].y);
        }
    }
    
    // 4. Подписи режима
    draw_mode_indicator(renderer, osc);
}

void draw_grid(SDL_Renderer *r, SDL_Rect rect, Oscilloscope *osc) {
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    
    // Вертикальные линии (время)
    int divs = 10; // 10 делений по горизонтали
    for (int i = 0; i <= divs; i++) {
        int x = rect.x + i * rect.w / divs;
        SDL_RenderDrawLine(r, x, rect.y, x, rect.y + rect.h);
        
        // Подпись времени (если есть шрифты)
        if (osc->mode == MODE_TIME_BASED) {
            float time = (i - divs/2) * osc->time_per_div;
            // draw_text(r, x, rect.y + rect.h - 10, "%.1fms", time*1000);
        }
    }
    
    // Горизонтальные линии (напряжение)
    for (int i = -5; i <= 5; i++) {
        int y = rect.y + rect.h/2 - i * rect.h/10;
        SDL_RenderDrawLine(r, rect.x, y, rect.x + rect.w, y);
    }
}

```


Cycle

``` cpp

int main() {
    Oscilloscope osc;
    SDL_Rect display_rect = {100, 100, 800, 400};
    
    // Инициализация...
    
    // Отдельный поток для сбора данных (или прерывание)
    // В этом потоке вызывается osc_update()
    
    while (running) {
        // Обработка ввода (переключение режимов)
        handle_input(&osc);
        
        // Рендер (использует кэш, построенный из последних данных)
        osc_render(&osc, renderer, display_rect);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

```


Переключение режимов - можно повесить на кнопку


``` cpp

void set_mode_time_based(Oscilloscope *osc, float time_per_div, bool scroll) {
    osc->mode = MODE_TIME_BASED;
    osc->time_per_div = time_per_div;
    osc->scroll_enabled = scroll;
    osc->render_points_count = 0; // сброс кэша
}

void set_mode_period_based(Oscilloscope *osc, int periods) {
    osc->mode = MODE_PERIOD_BASED;
    osc->periods_to_show = periods;
    osc->render_points_count = 0;
}

```


Как адекватно выбрать размер буфера 2) как подружить частоту обновления кадров, частоту пролета цикла процессора и прочие штуки? Я наверняка буду получать на вход точки с временами которые будь иметь неодинаковую дельту по тикам или мс в буфер так и писать или делать усреднение под дельту t

 Как выбрать размер буфера

 ``` cpp

 // Расчёт буфера
int calculate_buffer_size(float slowest_time_per_div, float sampling_rate_hz) {
    // Самая медленная развёртка: например, 1 секунда на деление
    // 10 делений = 10 секунд на весь экран
    float max_window_sec = slowest_time_per_div * 10.0f;
    
    // Нужно как минимум 2 окна (для плавности)
    float needed_sec = max_window_sec * 2;
    
    // Количество сэмплов = время * частота дискретизации
    int buffer_size = (int)(needed_sec * sampling_rate_hz);
    
    // Ограничения
    if (buffer_size < 1000) buffer_size = 1000;   // минимум
    if (buffer_size > 100000) buffer_size = 100000; // максимум (400KB при float)
    
    return buffer_size;
}

// Пример: частота 1 кГц, медленная развёртка 1 с/дел
// buffer = (1 * 10 * 2) * 1000 = 20000 точек (80 KB) - отлично!

```

Проблема неравномерной дельты времени

```cpp

// Если ты сам генерируешь сигнал с фиксированной частотой
void add_sample_fixed_rate(Oscilloscope *osc, float value) {
    static Uint32 last_time = 0;
    Uint32 now = SDL_GetTicks();
    
    if (now - last_time >= SAMPLE_INTERVAL_MS) {
        osc->samples[osc->write_idx] = value;
        osc->timestamps[osc->write_idx] = now;
        osc->write_idx = (osc->write_idx + 1) % osc->buffer_size;
        last_time = now;
    }
}

// или


typedef struct {
    float target_frequency_hz;     // желаемая частота сэмплирования для отображения
    float accum_time_ms;      // накопленное время
    float accum_value;        // накопленное значение для усреднения
    int samples_in_accum;     // сколько сырых точек усреднено
} Resampler;

void resampler_add_raw(Resampler *r, Uint32 time_ms, float value) {
    static Uint32 last_raw_time = 0;
    
    if (last_raw_time == 0) {
        last_raw_time = time_ms;
        return;
    }
    
    float dt_ms = time_ms - last_raw_time;
    float target_dt_ms = 1000.0f / r->target_frequency_hz;
    
    r->accum_time_ms += dt_ms;
    r->accum_value += value;
    r->samples_in_accum++;
    
    // Когда накопили достаточно времени - выдаём усреднённую точку
    while (r->accum_time_ms >= target_dt_ms) {
        float avg_value = r->accum_value / r->samples_in_accum;
        
        // Сохраняем в осциллограф
        osc_add_sample(osc, last_raw_time, avg_value);
        
        // Переходим к следующему окну
        r->accum_time_ms -= target_dt_ms;
        r->accum_value = 0;
        r->samples_in_accum = 0;
    }
    
    last_raw_time = time_ms;
}

// или


// При отрисовке интерполируем между реальными точками
void draw_irregular_samples(Oscilloscope *osc, SDL_Rect rect) {
    // Проходим по всем точкам в окне
    for (int i = 1; i < osc->render_points_count; i++) {
        Uint32 t1 = osc->timestamps[i-1];
        Uint32 t2 = osc->timestamps[i];
        float v1 = osc->samples[i-1];
        float v2 = osc->samples[i];
        
        // Рисуем отрезок - он автоматически учитывает неравномерность
        int x1 = time_to_x(t1, rect);
        int x2 = time_to_x(t2, rect);
        int y1 = value_to_y(v1, rect);
        int y2 = value_to_y(v2, rect);
        
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
}


```


Проблема синхронизации: Update vs Render vs Реальность


```cpp
typedef struct {
    // Поток сбора данных
    SDL_Thread *acquisition_thread;
    SDL_mutex *data_mutex;
    
    // Два буфера (double buffering)
    float *back_buffer_samples;    // пишем сюда из потока
    Uint32 *back_buffer_times;
    int back_buffer_count;
    
    float *front_buffer_samples;   // рендерим отсюда
    Uint32 *front_buffer_times;
    int front_buffer_count;
    
    // Флаги
    volatile bool new_data_ready;
} DoubleBuffer;

// ПОТОК СБОРА ДАННЫХ (высокий приоритет, реальное время)
int acquisition_thread_func(void *data) {
    DoubleBuffer *db = (DoubleBuffer*)data;
    
    while (running) {
        Uint32 start = SDL_GetTicks();
        
        // Читаем АЦП или получаем данные
        float value = read_adc();
        Uint32 now = SDL_GetTicks();
        
        // Блокируем и пишем в задний буфер
        SDL_LockMutex(db->data_mutex);
        db->back_buffer_samples[db->back_buffer_count] = value;
        db->back_buffer_times[db->back_buffer_count] = now;
        db->back_buffer_count++;
        SDL_UnlockMutex(db->data_mutex);
        
        // Дожидаемся следующего сэмпла (не жёстко, но стараемся)
        int elapsed = SDL_GetTicks() - start;
        if (elapsed < SAMPLE_INTERVAL_MS) {
            SDL_Delay(SAMPLE_INTERVAL_MS - elapsed);
        }
    }
    return 0;
}

// ФУНКЦИЯ ОБНОВЛЕНИЯ (вызывается из главного потока перед рендером)
void swap_buffers(DoubleBuffer *db) {
    SDL_LockMutex(db->data_mutex);
    
    if (db->back_buffer_count > 0) {
        // Меняем буферы местами
        float *temp_samples = db->front_buffer_samples;
        Uint32 *temp_times = db->front_buffer_times;
        
        db->front_buffer_samples = db->back_buffer_samples;
        db->front_buffer_times = db->back_buffer_times;
        db->front_buffer_count = db->back_buffer_count;
        
        db->back_buffer_samples = temp_samples;
        db->back_buffer_times = temp_times;
        db->back_buffer_count = 0;
        
        db->new_data_ready = true;
    }
    
    SDL_UnlockMutex(db->data_mutex);
}

// ГЛАВНЫЙ ЦИКЛ

int main() {
    DoubleBuffer db;
    init_double_buffer(&db, 50000);
    
    // Запускаем поток сбора данных
    SDL_CreateThread(acquisition_thread_func, "ADC", &db);
    
    while (running) {
        // 1. Обработка ввода
        handle_input();
        
        // 2. Обмен буферов (берём новые данные)
        swap_buffers(&db);
        
        // 3. Рендерим то, что в переднем буфере
        if (db.new_data_ready) {
            osc_render_from_buffer(&db, renderer);
            db.new_data_ready = false;
        }
        
        // 4. Стабильный FPS (ограничиваем)
        static Uint32 last_render = 0;
        Uint32 now = SDL_GetTicks();
        int frame_time = 1000 / TARGET_FPS;
        if (now - last_render < frame_time) {
            SDL_Delay(frame_time - (now - last_render));
        }
        last_render = SDL_GetTicks();
    }
}

```


Не усредняй время, если дельта нерегулярная — рисуй отрезками

Буфер = частота × максимальное время окна × 2

Не пытайся синхронизировать сбор и рендер — используй два буфера

Начни просто, усложняй по мере необходимости




Автоматически решает проблему неравномерной дельты

``` cpp

// У тебя есть реальные измерения
Точка А: (t=10мс, V=0.5В)
Точка Б: (t=25мс, V=0.8В)

// Ты просто соединяешь их прямой линией
// Это честно показывает, что между 10 и 25 мс у тебя нет данных

// Твои данные могут приходить с разными интервалами:
// 2мс, 5мс, 3мс, 10мс, 1мс...

// Просто рисуешь отрезки между соседними точками
int x1 = time_to_x(timestamps[i-1]);
int x2 = time_to_x(timestamps[i]);
SDL_RenderDrawLine(renderer, x1, y1, x2, y2);

// Широкий промежуток (10мс) нарисуется длинной наклонной линией
// Короткий промежуток (1мс) - короткой
// Это правильно отражает реальность!


```


Переполнение экрана

``` cpp

// Простой антиалиасинг для отрисовки
void draw_with_thick_lines(SDL_Renderer *r, int x1, int y1, int x2, int y2) {
    // Рисуем линию толщиной 2-3 пикселя (менее заметны артефакты)
    for (int dy = -1; dy <= 1; dy++) {
        SDL_RenderDrawLine(r, x1, y1 + dy, x2, y2 + dy);
    }
}

// Или используй режим сглаживания SDL
SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  // линейная интерполяция

```