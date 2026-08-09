Инструкция по проектированию программного осциллографа

1) Архитектура данных


Кольцевой буфер

``` cpp

typedef struct {
    float *samples;          // значения сигнала
    Uint32 *timestamps;      // временные метки (мкс)
    int size;                // ёмкость буфера
    volatile int head;       // индекс записи (атомарный)
    int count;               // количество накопленных точек
} RingBuffer;

// Расчёт размера буфера
int calculate_buffer_size(float slowest_time_per_div, float sampling_rate_hz) {
    float max_window_sec = slowest_time_per_div * 10.0f;  // 10 делений
    float needed_sec = max_window_sec * 2;                // минимум 2 окна
    int size = (int)(needed_sec * sampling_rate_hz);
    
    // Ограничения
    if (size < 1000) size = 1000;
    if (size > 100000) size = 100000;
    
    return size;
}

```


Два режима отображения

``` cpp

typedef enum {
    MODE_TIME_BASED,     // фиксированная временная развёртка
    MODE_PERIOD_BASED    // фиксированное количество периодов
} ScopeMode;

typedef struct {
    ScopeMode mode;
    bool scroll_enabled;        // только для TIME_BASED (уход влево)
    float time_per_div;          // секунд на деление
    int periods_to_show;         // для PERIOD_BASED (1, 2, 4...)
} DisplayParams;

```




Детектор периода (переход через ноль)

``` cpp

typedef struct {
    float trigger_level;         // обычно 0.0
    float last_value;
    Uint32 last_trigger_time_us;
    float detected_frequency_hz;
    float detected_period_ms;
} TriggerDetector;

void trigger_update(TriggerDetector *t, float value, Uint32 time_us) {
    if (t->last_value <= t->trigger_level && value > t->trigger_level) {
        if (t->last_trigger_time_us != 0) {
            float period_us = time_us - t->last_trigger_time_us;
            t->detected_period_ms = period_us / 1000.0f;
            t->detected_frequency_hz = 1000000.0f / period_us;
        }
        t->last_trigger_time_us = time_us;
    }
    t->last_value = value;
}

```


Реализация (один поток с проверками)

``` cpp

void run_zones() {
    const int RT_INTERVAL_US = 1000000 / SAMPLE_RATE_HZ;  // 16 мкс при 60 кГц
    const int RENDER_INTERVAL_MS = 16;     // 60 FPS
    const int ANALYTICS_INTERVAL_MS = 100; // 10 Гц
    const int CLOCK_INTERVAL_MS = 1000;    // 1 Гц
    
    Uint32 last_rt_us = 0;
    Uint32 last_render_ms = 0;
    Uint32 last_analytics_ms = 0;
    Uint32 last_clock_ms = 0;
    
    while (running) {
        Uint32 now_us = get_precise_time_us();
        Uint32 now_ms = now_us / 1000;
        
        // ЗОНА 1: Реального времени (самая частая)
        if (now_us - last_rt_us >= RT_INTERVAL_US) {
            float value = read_adc();
            ringbuffer_push(&buffer, value, now_us);
            trigger_update(&trigger, value, now_us);
            last_rt_us = now_us;
        }
        
        // ЗОНА 2: Рендер
        if (now_ms - last_render_ms >= RENDER_INTERVAL_MS) {
            render_all();
            SDL_RenderPresent();
            last_render_ms = now_ms;
        }
        
        // ЗОНА 3: Аналитика
        if (now_ms - last_analytics_ms >= ANALYTICS_INTERVAL_MS) {
            update_analytics();
            last_analytics_ms = now_ms;
        }
        
        // ЗОНА 4: Часы (редко)
        if (now_ms - last_clock_ms >= CLOCK_INTERVAL_MS) {
            update_clock_display();
            last_clock_ms = now_ms;
        }
        
        // Микро-сон для снижения нагрузки CPU
        precise_sleep_us(100);  // 0.1 мс
    }
}

```

Отдельные потоки

``` cpp

// ПОТОК 1: Realtime (приоритет = HIGH)
void* rt_thread(void*) {
    while (running) {
        Uint32 start = get_time_us();
        float value = read_adc();
        ringbuffer_push(&buffer, value, start);
        
        int elapsed = get_time_us() - start;
        if (elapsed < RT_INTERVAL_US) {
            precise_sleep_us(RT_INTERVAL_US - elapsed);
        }
    }
}

// ПОТОК 2: Render (приоритет = NORMAL)
void* render_thread(void*) {
    while (running) {
        render_all();
        SDL_RenderPresent();
        SDL_Delay(16);
    }
}

// ПОТОК 3: Analytics (приоритет = LOW)
void* analytics_thread(void*) {
    while (running) {
        sleep_ms(100);
        ringbuffer_snapshot(&snapshot);  // копируем данные
        calculate_fft(snapshot);
        update_stats();
    }
}

```

Интерполляция отрезками

``` cpp

void render_waveform(RingBuffer *buf, SDL_Rect rect, DisplayParams *params) {
    Uint32 now_ms = SDL_GetTicks();
    Uint32 time_start = calculate_time_window(now_ms, params);
    
    int prev_x = -1, prev_y = -1;
    
    for (int i = 0; i < buf->count; i++) {
        int idx = get_sample_index(buf, i);  // от новых к старым
        
        if (buf->timestamps[idx] < time_start) continue;
        
        int x = time_to_x(buf->timestamps[idx], time_start, rect, params);
        int y = value_to_y(buf->samples[idx], rect);
        
        if (prev_x != -1) {
            SDL_RenderDrawLine(renderer, prev_x, prev_y, x, y);
        }
        
        prev_x = x;
        prev_y = y;
    }
}

```

Рендер-кэш

``` cpp

typedef struct {
    SDL_Point *points;
    int count;
    bool dirty;
    Uint32 cached_time_window;
    float cached_time_per_div;
} RenderCache;

void render_with_cache(RingBuffer *buf, RenderCache *cache, SDL_Rect rect, DisplayParams *params) {
    // Перестраиваем кэш только при изменении параметров или новых данных
    if (cache->dirty || 
        cache->cached_time_window != params->time_window_ms ||
        cache->cached_time_per_div != params->time_per_div) {
        
        rebuild_cache(buf, cache, rect, params);
        cache->dirty = false;
    }
    
    // Рисуем из кэша
    for (int i = 1; i < cache->count; i++) {
        SDL_RenderDrawLine(renderer, 
            cache->points[i-1].x, cache->points[i-1].y,
            cache->points[i].x, cache->points[i].y);
    }
}

```

Сетка

```

void render_grid(SDL_Rect rect, DisplayParams *params) {
    // 10x10 делений
    int h_divs = 10, v_divs = 10;
    
    for (int i = 0; i <= h_divs; i++) {
        int x = rect.x + i * rect.w / h_divs;
        SDL_RenderDrawLine(renderer, x, rect.y, x, rect.y + rect.h);
        
        // Подпись времени
        if (params->mode == MODE_TIME_BASED) {
            float time = (i - h_divs/2) * params->time_per_div;
            draw_text(x, rect.y + rect.h - 10, "%.1f", time);
        }
    }
    
    for (int i = -v_divs/2; i <= v_divs/2; i++) {
        int y = rect.y + rect.h/2 - i * rect.h / v_divs;
        SDL_RenderDrawLine(renderer, rect.x, y, rect.x + rect.w, y);
        
        char label[16];
        snprintf(label, sizeof(label), "%.1fV", i * params->volt_per_div);
        draw_text(rect.x + 5, y - 5, label);
    }
}

```


Управление памятью

``` cpp

// ПЛОХО (в реальном времени):
void rt_bad(float value) {
    float *temp = malloc(sizeof(float));  // ← НЕЛЬЗЯ!
    // ...
}

// ХОРОШО (всё предварительно выделено):
RingBuffer buffer;  // глобальный или переданный
void rt_good(float value) {
    buffer.samples[buffer.head] = value;  // просто запись в готовый массив
}

```


Двойная буферизация

``` cpp

typedef struct {
    RingBuffer back;    // пишет Realtime-поток
    RingBuffer front;   // читает Render-поток
    SDL_mutex *mutex;
    volatile bool swap_needed;
} DoubleBuffer;

void swap_buffers(DoubleBuffer *db) {
    SDL_LockMutex(db->mutex);
    RingBuffer temp = db->front;
    db->front = db->back;
    db->back = temp;
    db->swap_needed = false;
    SDL_UnlockMutex(db->mutex);
}

```


Претриггер

``` cpp

typedef struct {
    RingBuffer *main_buffer;
    float *pretrigger;           // отдельный кольцевой буфер
    int pretrigger_size;         // количество точек до триггера
    int pretrigger_head;
    bool armed;                  // вооружён (ждём триггер)
} Pretrigger;

void pretrigger_add(Pretrigger *p, float value, Uint32 time_us) {
    // Всегда пишем в претриггерный буфер
    p->pretrigger[p->pretrigger_head] = value;
    p->pretrigger_head = (p->pretrigger_head + 1) % p->pretrigger_size;
    
    if (p->armed && trigger_detected(value)) {
        // Триггер сработал! Копируем претриггер в основной буфер
        for (int i = 0; i < p->pretrigger_size; i++) {
            int idx = (p->pretrigger_head + i) % p->pretrigger_size;
            ringbuffer_push(p->main_buffer, p->pretrigger[idx], time_us);
        }
        p->armed = false;
    }
}

```


Режим нескольких периодов (Persist)

``` cpp

typedef struct {
    float **periods;          // массив указателей на периоды
    int max_periods;          // сколько храним
    int current_period;       // индекс текущего периода
    int samples_per_period;   // сколько точек в периоде
    float *temp_period;       // временный буфер для накопления
    int temp_count;
} PeriodBuffer;

void period_add_sample(PeriodBuffer *p, float value, TriggerDetector *t) {
    p->temp_period[p->temp_count++] = value;
    
    if (trigger_detected(t, value)) {
        // Период закончен, сохраняем
        if (p->temp_count > 10) {
            // Копируем в periods[p->current_period]
            memcpy(p->periods[p->current_period], p->temp_period, 
                   p->temp_count * sizeof(float));
            p->samples_per_period = p->temp_count;
            p->current_period = (p->current_period + 1) % p->max_periods;
        }
        p->temp_count = 0;
    }
}

// Отрисовка: рисуем ВСЕ периоды наложенными
void period_render(PeriodBuffer *p, SDL_Rect rect) {
    for (int i = 0; i < p->max_periods && i < periods_to_show; i++) {
        int idx = (p->current_period - i + p->max_periods) % p->max_periods;
        draw_waveform(p->periods[idx], p->samples_per_period, rect);
    }
}

```


Обработка неравномерной дельты времени

``` cpp

// Ресемплинг (приведение к равномерной сетке)
typedef struct {
    float target_frequency_hz;       // целевая частота
    float accum_time_us;        // накопленное время
    float accum_value;          // накопленное значение
    int accum_count;            // сколько сырых точек усреднено
} Resampler;

bool resampler_add(Resampler *r, float value, Uint32 time_us, float *out_value) {
    static Uint32 last_time_us = 0;
    float target_dt_us = 1000000.0f / r->target_frequency_hz;
    
    if (last_time_us == 0) {
        last_time_us = time_us;
        return false;
    }
    
    float dt_us = time_us - last_time_us;
    r->accum_time_us += dt_us;
    r->accum_value += value;
    r->accum_count++;
    
    if (r->accum_time_us >= target_dt_us) {
        *out_value = r->accum_value / r->accum_count;
        r->accum_time_us -= target_dt_us;
        r->accum_value = 0;
        r->accum_count = 0;
        last_time_us = time_us;
        return true;  // есть новый ресемплированный сэмпл
    }
    
    last_time_us = time_us;
    return false;
}

```


2. Чек-лист при проектировании
   
Начальный этап
Определить максимальную частоту дискретизации (например, 60 кГц)

Рассчитать размер буфера (время развёртки × частота × 2)

Выбрать метод отображения (начала с отрезков между точками)

Архитектура
Разделить на 4 временные зоны (Realtime, Render, Analytics, Clock)

Realtime-зона: только запись в буфер, без аллокаций

Render-зона: рендер-кэш, перестраивается при изменении параметров

Analytics-зона: копирует данные перед обработкой

Отображение
Реализовать два режима (временная развёртка и периодная)

Сделать переключение режимов без задержек

Претриггер для ловли событий

Оптимизация
Рендер-кэш (не перестраивать каждый кадр)

Двойная буферизация для данных

Атомарные операции для head/index

Децимация при слишком большом количестве точек (>2x от ширины экрана)


3. Типичные ошибки и их решения

Ошибка	Решение

malloc в realtime-зоне	| Предварительное выделение всех буферов

Обновление UI 60 000 раз/сек | UI только в render-зоне (60 FPS) или реже

Долгие блокировки мьютексов | lock-free структуры или очень короткие блокировки

Рисование всех точек каждый кадр | Рендер-кэш, перестраивать при изменении

Тяжёлая математика в realtime | Отдельный analytics-поток

CPU 100% постоянно | Микро-сон между итерациями цикла
