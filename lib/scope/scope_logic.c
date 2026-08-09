// scope_logic.c

#define TEST_MODE_0 0 // Обнуление при on-off
#define TEST_MODE_1 0 // Общий тест
#define TEST_MODE_2 0 // Тест EMA-пайплайна
#define TEST_MODE_3 0 // Тест MIN-MAX-пайплайна
#define TEST_MODE_4 0 // Тест zc-детектор пайплайна
#define TEST_MODE_5 0 // Тест pattern-детектор пайплайна
#define TEST_MODE_6 0 // Новый запуск анализа после on-off

// =========================================================================================== IMPORT

#include "scope_internal.h"

#include <stdlib.h>

#include "../app_timer/app_timer.h"
#include "../my_generator/my_generator.h"


#include "../../src/global_data.h"

#include <math.h>

#include <float.h>

#include <stdio.h>


// =========================================================================================== IMPORT


/*

    ЧТО ЛЕЖИТ В ЭТОМ ФАЙЛЕ

    Вся "внутренность" осциллографа, не связанная с отрисовкой:

        1) инициализация и очистка всех контекстов;
        2) приём сигнала в кольцевой буфер (scope_buffer_update);
        3) рантайм-анализ каждого сэмпла - центр, шум, порог, тренды, пики
           (runtime_data_update -> runtime_detect_trends);
        4) детектор полуволн на стейт-машине (runtime_detect_halfwaves);
        5) медленный анализ - паттерн, период, measured-характеристики, подстройка фильтра;
        6) публичный API объекта - scope_init / scope_fast_update /
           scope_slow_update / scope_destroy.

    Отрисовка, геометрия корпуса и коллбеки кнопок вынесены в scope_gui.c.

*/


// =========================================================================================== INIT and CLEAR helpers

void scope_main_settings_init(Scope* used_scope)
{
    // ===== Инициализация основных настроек ===== 

    used_scope->main_settings.current_state = OFF_SS;
    used_scope->main_settings.current_mode = SCOPE_MODE_FIXED_TIME_STEP_SRM;            // Базово - фикс
    

    used_scope->main_settings.periods_to_display = 2;                                   // Базово - 2 периода для отображения (в режиме с фикс. кол-вом)
    

    used_scope->main_settings.current_signal_units  = VOLTS_SU;                         // Базово - вольты 
    used_scope->main_settings.current_time_units = MILLISECONDS_TU;                     // Базово - миллисекунда (но переменная всегда в секундах)

    used_scope->main_settings.current_time_in_unit_steps = TIUS_1_MS;
    used_scope->main_settings.time_val_in_one_unit = 1;                                 // Базово - 1 (режим с фикс. разв)
    used_scope->main_settings.signal_val_in_one_unit = 2;                               // Базово - 1 (режим с фикс. разв)
    


    // ===== Инициализация основных настроек ===== 


    // ===== Сигнал ===== 

    // No signal at the start
    used_scope->signal_control_data.controlled_signal = NULL;

    // Не первый вызов (вкл - выкл)
    if (used_scope->signal_control_data.prev_call_time != 0.0f) 
        used_scope->signal_control_data.prev_call_time = simulation_timer_get_time() - simulation_timer_get_time_step();

    else used_scope->signal_control_data.prev_call_time = 0.0f;


    // ===== Сигнал ===== 
}


void scope_signal_buffer_init(Scope* used_scope)
{
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    memset(buffer->samples, 0, sizeof(buffer->samples));


    buffer->head = 0;
    buffer->count = 0;
}


void scope_running_signal_characteristics_init(Scope* used_scope)
{
    scope_running_signal_data_ctx* running_data = &used_scope->signal_control_data.running_signal_characteristics;

    // =========================================================
    // 1. MEAN (EMA Фильтр)
    // =========================================================
    running_data->running_mean.mean = 0.0f;

    // Задаем физическое время сглаживания (подавление частот выше ~500 Гц)
    float target_smoothing_time = 0.002f; // 2 миллисекунды

    // Находим эквивалентное окно в количестве семплов: N = time * f_s
    float samples_in_window = target_smoothing_time * (float)SCOPE_SAMPLE_RATE; 

    // Защита от деления на ноль, если частота дискретизации выставлена неверно
    if (samples_in_window < 1.0f) samples_in_window = 1.0f;

    // Финальный расчет коэффициента затухания по формуле: alpha = 2 / (N + 1)
    running_data->running_mean.alpha = 2.0f / (samples_in_window + 1.0f);


    // =========================================================
    // 2. MEDIAN (Sign-LMS со вторым порядком интеграции)
    // =========================================================
    running_data->running_median.median = 0.0f;

    /*

        ИСПРАВЛЕНО: шаг медианы.

        Раньше шаг считался от МАКСИМАЛЬНОЙ контролируемой частоты:

            v_max = 1 * 2 * PI * MAX_CONTROLLED_FREQ            // 20 кГц
            step  = v_max / SCOPE_SAMPLE_RATE * 0.001

        что давало примерно 0.0026 В на сэмпл, то есть 125 В/с.

        Медиана - это оценка ПОСТОЯННОЙ составляющей, она обязана двигаться
        медленнее самого сигнала. При 125 В/с за половину периода сигнала
        196 Гц (122 сэмпла) медиана уезжала на 0.32 В. Для сигнала амплитудой
        5 В это 6 процентов размаха: центр "дышал" вместе с волной, а вместе
        с ним ездили и точки перехода через ноль. Отсюда разъезжались
        длительности полуволн, а за ними и период.

        Правильный ориентир - не максимальная частота сигнала, а время
        слежения за дрейфом нуля. Задаём его явно через DC_TRACKING_TIME:
        медиана должна проходить весь ожидаемый размах сигнала примерно
        за это время.

        Позже, когда амплитуда и период уже измерены, renew_filter уточняет
        шаг так, чтобы уход медианы за половину периода не превышал
        MAX_MEDIAN_DRIFT_PART от амплитуды.

    */

    running_data->running_median.step =
        EXPECTED_SIGNAL_RANGE / (DC_TRACKING_TIME * (float)SCOPE_SAMPLE_RATE);


    // Начальное значение дрифта (скорости изменения) ставим в ноль,
    // чтобы алгоритм стартовал из стабильного состояния и набрал скорость сам.
    running_data->running_median.drift = 0.0f;


    // =========================================================
    // 3. FUSION & OFFSET MANAGEMENT
    // =========================================================
    running_data->median_part_in_offset = 0.9f;
    running_data->mean_part_in_offset = 0.1f;
    running_data->running_dc_offset = 0.0f;


    running_data->last_not_noise_value = 0.0f;
    running_data->last_not_noise_time = 0.0f;
}


void scope_measured_signal_characteristics_init(Scope* used_scope)
{
    scope_measured_signal_data_ctx* measured_data = &used_scope->signal_control_data.measured_signal_characteristics;


    measured_data->current_confidence_to_running = 1.0f;     


    measured_data->measured_periods.head = 0;
    measured_data->measured_periods.count = 0;
    
    for (int i = 0; i < PERIOD_DETECTOR_BUFFER_SIZE / 4; i++)
    {
        measured_data->measured_periods.periods[i] = 0.0f;
    }


    measured_data->measured_period = 0.0f;
    measured_data->measured_frequency = 0.0f;

    measured_data->measured_mean = 0.0f;
    measured_data->measured_median = 0.0f;

    measured_data->measured_max = -FLT_MAX;
    measured_data->measured_min = FLT_MAX;

    measured_data->measured_amplitude = 0.0f;

    measured_data->measured_dc_offset = 0.0f;

    measured_data->pattern_confidence = 0.0f;
    measured_data->pattern_halfwaves = 0;
}


void scope_filter_init(Scope* used_scope)
{
    scope_realtime_filtering_ctx* filter = &used_scope->signal_control_data.filter_ctx;

    // Оценка шума (variance/дисперсия) должна меняться медленнее, чем само среднее значение,
    // чтобы избежать ложных срабатываний динамического порога на случайных пиках.
    // Мы закладываем время интеграции шума 0.005 с. При частоте дискретизации 48000

    // Количество сэмплов окна дисперсии N = 0.005 * 48000
    // Betha = 2 / (N + 1) = 2 / (240 + 1) = 0.0083

    float noise_integration_time = 0.005f;
    float samples_in_noise_window = noise_integration_time * (float)SCOPE_SAMPLE_RATE;

    // Защита от некорректной частоты дискретизации
    if (samples_in_noise_window < 1.0f) samples_in_noise_window = 1.0f;

    // Находим betha по формуле экспоненциального скользящего среднего: 2 / (N + 1)
    filter->running_betha = 2.0f / (samples_in_noise_window + 1.0f);

    // НАЧАЛЬНОЕ СОСТОЯНИЕ ШУМА:
    // Безопаснее стартовать с небольшого шума, отличного от нуля,
    // чтобы running_treshold сразу имел адекватную зону мертвой полосы.
    // Если running_sigma_squad — это СКО, оставляем 0.01f.
    // Если в структуре лежит квадрат (дисперсия), то пишем (0.01f * 0.01f).
    filter->running_sigma_squad = 0.01f * 0.01f;


    /*

        ОТДЕЛЬНАЯ МОДЕЛЬ ШУМА

        running_sigma_squad - это дисперсия ПОЛНОГО сигнала относительно центра.
        Для периодического сигнала она равна примерно A^2 / 2 и о шуме не говорит
        вообще ничего: чистая синусоида амплитудой 5 В даст сигму 3.5 В.

        Поэтому мощность именно шума оценивается отдельно, по второй разности:

            hf[n] = x[n] - 2*x[n-1] + x[n-2]

        Вторая разность обнуляет постоянную и линейную составляющие, то есть
        для гладкого сигнала на частоте много ниже частоты дискретизации она
        близка к нулю. Для некоррелированного шума её дисперсия равна
        6 * sigma_noise^2, откуда прямая оценка мощности шума за O(1).

        Приём известен как noise estimation via second difference (в астрономии
        встречается под именем DER_SNR). Он и даёт ту адаптивность, которая
        изначально задумывалась: чисто - порог маленький, шумно - порог растёт.

    */

    float noise_samples_in_window = NOISE_ESTIMATION_TIME * (float)SCOPE_SAMPLE_RATE;

    if (noise_samples_in_window < 1.0f) noise_samples_in_window = 1.0f;

    filter->noise_betha = 2.0f / (noise_samples_in_window + 1.0f);

    filter->noise_sigma_squad = 0.0f;

    filter->prev_value_1 = 0.0f;
    filter->prev_value_2 = 0.0f;

    filter->filter_warmup_counter = 0;


    // Множитель правила от 0.5 до 3 сигм (отсекает 99.7% случайных пиков шума)
    filter->k_treshold = 3.0f;

    // Рассчитываем стартовый порог сразу при инициализации.
    // Стартовое значение берём из ZC_TRESHOLD_START_VALUE - до появления
    // первых данных амплитуда ещё неизвестна, а порог уже нужен
    filter->running_treshold = ZC_TRESHOLD_START_VALUE;
}


void scope_peaks_ctx_init(Scope* used_scope)
{
    scope_realtime_peaks_ctx* peaks_ctx = &used_scope->signal_control_data.peaks_ctx;

    peaks_ctx->prev_trend = FALLING_PT;         // Предыдущий тренд сигнала - предполагается falling, сменится сразу после

    peaks_ctx->trend_confidence = 1.0f;         // Максимальное доверие к первому тренду
    peaks_ctx->last_event_confidence = 1.0f;    // Максимальное доверие к прошлому ивенту

    peaks_ctx->peak_candidate = -FLT_MAX;       // Кандидат на пик
    peaks_ctx->trough_candidate = FLT_MAX;      // Кандидат на яму

    peaks_ctx->last_peak = -FLT_MAX;            // Прошлый пик
    peaks_ctx->last_trough = FLT_MAX;           // Прошлая яма

    peaks_ctx->max_candidate = -FLT_MAX;        // Кандидат на максимум
    peaks_ctx->min_candidate = FLT_MAX;         // Кандидат на минимум

    peaks_ctx->max_confidence = 1.0f;           // Уверенность в кандидате
    peaks_ctx->min_confidence = 1.0f;           // Уверенность в кандидате


    peaks_ctx->running_max = -FLT_MAX;          // Текущее максимальное значение
    peaks_ctx->running_min = FLT_MAX;           // Текущее минимальное значение

    peaks_ctx->running_amplitude = 0.0f;     // Текущая амплитуда

}


void scope_wave_pattern_detector_former_init(Scope* used_scope)
{
    wave_pattern_detector_former_ctx* wpdf_ctx = &used_scope->signal_control_data.wave_pattern_detector_former_data;

    // Определяем первичным состоянием former'а ожидание восходящего zero-cross с любой позиции приёма первых данных
    // чтобы первой фиксируемой полу
    wpdf_ctx->prev_signal_position = INSIDE_ZC_TZ_SP;           // Предыдущая позиция сигнала относительно dc_offset +- treshold
    wpdf_ctx->prev_buffer_former_state = WAIT_RISING_MINUS_FS;  // Предыдущее состояние former'а (для определения момента смены состояния)

    // Ожидание восходящего zero-cross с любой позиции приёма первых данных
    wpdf_ctx->buffer_former_state = LIMIT_FS;


    // Инициализация пустой полуволны (часть данных уйдёт под замену при первом же восходящем zero cross, часть при первом достижении STATIC_PT)
    wpdf_ctx->curr_halfwave.halfwave_type = STATIC_PT;
    wpdf_ctx->curr_halfwave.start_time = 0.0f;
    wpdf_ctx->curr_halfwave.end_time = 0.0f;

    wpdf_ctx->curr_halfwave.halfwave_full_time = 0.0f;

    wpdf_ctx->curr_halfwave.peak_value = -FLT_MAX;
    wpdf_ctx->curr_halfwave.trough_value = FLT_MAX;
    wpdf_ctx->curr_halfwave.halfwave_area = 0.0f;
    wpdf_ctx->curr_halfwave.halfwave_smoothed_speed = 0.0f;
    wpdf_ctx->curr_halfwave.samples_in_halfwave = 0;


    wpdf_ctx->halfwave_zero_crosses[0].zero_cross_type = STATIC_PT;
    wpdf_ctx->halfwave_zero_crosses[0].time = 0.0f;
    wpdf_ctx->halfwave_zero_crosses[0].filled = false;

    wpdf_ctx->halfwave_zero_crosses[1].zero_cross_type = STATIC_PT;
    wpdf_ctx->halfwave_zero_crosses[1].time = 0.0f;
    wpdf_ctx->halfwave_zero_crosses[1].filled = false;

    // Первично доступ открыт
    wpdf_ctx->accumulation_block = false;

    wpdf_ctx->prev_clean_signal_value = 0.0f;
    wpdf_ctx->prev_clean_signal_time = 0.0f;
}


void scope_wave_pattern_detector_init(Scope* used_scope)
{
    wave_pattern_detector_ctx* wpd_ctx = &used_scope->signal_control_data.wave_pattern_detector_data;

    wpd_ctx->head = 0;
    wpd_ctx->count = 0;


    for (int i = 0; i < PERIOD_DETECTOR_BUFFER_SIZE; i++)
    {
        wpd_ctx->halfwaves_for_detection[i].halfwave_type = STATIC_PT;
        wpd_ctx->halfwaves_for_detection[i].start_time = 0.0f;
        wpd_ctx->halfwaves_for_detection[i].end_time = 0.0f;
    
        wpd_ctx->halfwaves_for_detection[i].halfwave_full_time = 0.0f;
    
        wpd_ctx->halfwaves_for_detection[i].peak_value = -FLT_MAX;
        wpd_ctx->halfwaves_for_detection[i].trough_value = FLT_MAX;
        wpd_ctx->halfwaves_for_detection[i].halfwave_area = 0.0f;
        wpd_ctx->halfwaves_for_detection[i].halfwave_smoothed_speed = 0.0f;
        wpd_ctx->halfwaves_for_detection[i].samples_in_halfwave = 0;
    }
}

// Clear

void scope_main_settings_clear(Scope* used_scope)
{
    // ===== Инициализация основных настроек ===== 

    used_scope->main_settings.current_state = OFF_SS;
    used_scope->main_settings.current_mode = SCOPE_MODE_FIXED_TIME_STEP_SRM;            // Базово - фикс
    
    used_scope->main_settings.periods_to_display = 2;                                   // Базово - 2 периода для отображения (в режиме с фикс. кол-вом)
    

    used_scope->main_settings.current_signal_units  = VOLTS_SU;                         // Базово - вольты 
    used_scope->main_settings.current_time_units = MICROSECONDS_TU;                     // Базово - микросекунды (но переменная всегда в секундах)


    used_scope->main_settings.current_time_in_unit_steps = TIUS_1_MS;
    used_scope->main_settings.time_val_in_one_unit = 1;                                 // Базово - 1 (режим с фикс. разв)
    used_scope->main_settings.signal_val_in_one_unit = 1;                               // Базово - 1 (режим с фикс. разв)
    

    // ===== Инициализация основных настроек ===== 

    // Не первый вызов (вкл - выкл)
    if (used_scope->signal_control_data.prev_call_time != 0.0f) 
        used_scope->signal_control_data.prev_call_time = simulation_timer_get_time() - simulation_timer_get_time_step();

    else used_scope->signal_control_data.prev_call_time = 0.0f;


    // ===== Инициализация основных настроек ===== 
}


void scope_signal_buffer_clear(Scope* used_scope)
{
    // Repeat init
    scope_signal_buffer_init(used_scope);
}



void scope_running_signal_characteristics_clear(Scope* used_scope)
{
    // Repeat init
    scope_running_signal_characteristics_init(used_scope);
}


void scope_measured_signal_characteristics_clear(Scope* used_scope)
{
    // Repeat init
    scope_measured_signal_characteristics_init(used_scope);
}


void scope_filter_clear(Scope* used_scope)
{
    // Repeat init
    scope_filter_init(used_scope);
}


void scope_peaks_ctx_clear(Scope* used_scope)
{
    // Repeat init
    scope_peaks_ctx_init(used_scope);
}


void scope_wave_pattern_detector_former_clear(Scope* used_scope)
{
    // Repeat init
    scope_wave_pattern_detector_former_init(used_scope);
}


void scope_wave_pattern_detector_clear(Scope* used_scope)
{   
    // Repeat init
    scope_wave_pattern_detector_init(used_scope);
}

// =========================================================================================== INIT and CLEAR helpers




// Апдейт буффера с макс. скоростью
void scope_buffer_update(Scope* used_scope)
{
    // берёт текущее значение сигнала
    // записывает в кольцевой буфер
    // фиксирует time + delta_t
    // вызывает детектор событий (переход через 0)

    if (!used_scope) return;

    scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
    scope_buffer_ctx* buffer = &ctrl->scope_buffer_data;


    if (!ctrl->controlled_signal) return;

    // Ничего не делаем, если выключен
    if (used_scope->main_settings.current_state == OFF_SS) return;

    // ===== Получение значений =====
    

    // Текущее время - точно совпадёт со временем, которое было принято на 
    // генерацию значения сигнала - смотрим в "прошлое"
    double curr_t = simulation_timer_get_time() - simulation_timer_get_time_step();


    // Шаг дискретизации 
    double delta_t = simulation_timer_get_time_step();
    double sample_delta_t = simulation_timer_get_sample_step();


    // Выбор сигнала 
    float* value;

    switch (ctrl->type_of_controlled_signal)
    {
        case CLEAN_CST:

            value = my_generator_get_clean();
            break;


        case NOISED_CST:

            value = my_generator_get_noise();
            break;


        default:

            printf("Reading error!\n");
            return;

    }


    // ===== Заполнение буффера =====
    for (int i = 0; i < SIM_BUFFER_SIZE; i++)
    {

        int head = buffer->head;

        // Предыдущее значение с зашитой от ошибки 1 шага
        int prev = head - 1;
        if (prev < 0)
            prev = BUFFER_SIZE - 1;


        buffer->samples[head].value = value[i];
        buffer->samples[head].time = curr_t + sample_delta_t * (i + 1);


        if (buffer->count > 0)
        {
            // Первый сэмпл после прохода через кольцо, или любой другой
            buffer->samples[head].delta_t = sample_delta_t;
        }
        else
        {
            // Первый сэмпл после init
            buffer->samples[head].delta_t = 0.0;
        }
        

        if (TEST_MODE_2) {

            printf("\nЗаписали в буффер значение: %lf\n", (double)buffer->samples[head].value);

        }


        // Update ring buffer
        buffer->head = (head + 1) % BUFFER_SIZE;

        // Сдвиг буффера
        if (buffer->count < BUFFER_SIZE) buffer->count++;
    }

    
    // Сдвиг предыдущего времени вызова
    used_scope->signal_control_data.prev_call_time += simulation_timer_get_time_step();
}



void runtime_data_update(Scope* used_scope)
{
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    // Функция читает сэмплы от head - SIM_BUFFER_SIZE - 1 и дальше вперёд,
    // поэтому в буфере должно лежать на один сэмпл больше, чем пришедшая пачка.
    // Проверки count < 2 для этого не хватало: на самом первом проходе
    // prev_idx уходил в конец кольца, где лежали ещё не записанные нули,
    // и весь первый пакет анализировался по мусорному предыдущему значению
    if (buffer->count < SIM_BUFFER_SIZE + 1) return;


    // For-style signal data update

    for (int i = 0; i < SIM_BUFFER_SIZE; i++)
    {
        // =========================================================
        // 0. RAW SIGNAL
        // =========================================================

        // Сигнал уже записан, head сдвинут, соответственно


        int curr_idx = buffer->head - SIM_BUFFER_SIZE + i;
        if (curr_idx < 0) curr_idx += BUFFER_SIZE;                  // Сдвиг при проходе кольца

        int prev_idx = buffer->head - SIM_BUFFER_SIZE + i - 1;
        if (prev_idx < 0) prev_idx += BUFFER_SIZE;

        // Сэмплы сигнала
        sample_t curr = buffer->samples[curr_idx];
        sample_t prev = buffer->samples[prev_idx];

        float curr_x = curr.value;
        double curr_t = curr.time;
        float prev_x = prev.value;


        if (TEST_MODE_3) {

            printf("\nNEW Curr: %f\n", curr_x);
            printf("Prev: %f\n", prev_x);
        }



        // =========================================================
        // 1. STATE (Связываем с вашими структурами контекста)
        // =========================================================

        scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
        scope_running_signal_data_ctx* running_data = &ctrl->running_signal_characteristics;
        scope_realtime_filtering_ctx*  filter_data = &ctrl->filter_ctx;
        scope_realtime_peaks_ctx* peaks_data = &ctrl->peaks_ctx;


        // =========================================================
        // 2. CENTER MODEL (running_mean / running_median / running_dc)
        // =========================================================

        // 2.1 running_mean (EMA)
        running_data->running_mean.mean += running_data->running_mean.alpha * (curr_x - running_data->running_mean.mean);


        if (TEST_MODE_2) {

            printf("Новый mean: %lf\n", (double)running_data->running_mean.mean);
            
        }


        // 2.2 RUNNING MEDIAN (robust center estimator via Sign-LMS)
        float error = curr_x - running_data->running_median.median;

        running_data->running_median.median += 
            running_data->running_median.step * copysignf(1.0f, error);


        if (TEST_MODE_2) {

            printf("Новый median: %lf\n", (double)running_data->running_median.median);
            
        }


        // 2.3 running_dc (fusion center)

        running_data->running_dc_offset = (

            (running_data->median_part_in_offset * running_data->running_median.median) + 
            (running_data->mean_part_in_offset * running_data->running_mean.mean)
            
        );


        if (TEST_MODE_2) {

            printf("Новый offset: %lf\n", (double)running_data->running_dc_offset);
            
        }


        // =========================================================
        // 3. NOISE MODEL (Интеграция дисперсии шума)
        // =========================================================

        /*

            ЧТО ЗДЕСЬ БЫЛО НЕ ТАК

            В прошлой версии на этом месте стояло:

                diff = fmaxf(running_amplitude * 0.1f, 0.1f);
                sigma_squad += betha * (diff*diff - sigma_squad);
                running_treshold = diff + k_treshold * sigma;

            То есть "отклонение" вообще не бралось из сигнала: это была просто
            десятая часть амплитуды. Следствия:

                1) sigma сходилась к константе 0.1 * A и никакого шума не мерила -
                   EMA-интегратор дисперсии работал вхолостую;

                2) порог получался равным (0.1 + 0.1 * k) * A, то есть при k = 0.5
                   это жёстко зашитые 15 процентов амплитуды;

                3) заявленная адаптивность не существовала: на чистом сигнале
                   порог был такой же широкий, как на шумном, а как только шум
                   переваливал за 15 процентов амплитуды - детектор рассыпался.
                   Ровно та граница Aш/Aс ~ 0.25, о которую всё упиралось.

            ЧТО СТАЛО

            Считаем две РАЗНЫЕ величины:

                running_sigma_squad - дисперсия полного сигнала относительно центра
                                      (нужна renew_filter и диагностике);

                noise_sigma_squad   - дисперсия ТОЛЬКО шума, по второй разности.

            Порог строится по второй, потому что именно она отвечает на вопрос
            "насколько сигнал дрожит сам по себе".

        */

        if (TEST_MODE_2)
        {
            printf("Текущая амплитуда: %lf\n", (double)peaks_data->running_amplitude);
        }


        // ----- 3.1 Дисперсия полного сигнала (EMA от квадрата отклонения) -----

        float diff = curr_x - running_data->running_dc_offset;

        float sigma_squad = filter_data->running_sigma_squad;

        sigma_squad += filter_data->running_betha * (diff * diff - sigma_squad);

        if (!isfinite(sigma_squad) || sigma_squad < 0.0f) sigma_squad = 0.0f;

        filter_data->running_sigma_squad = sigma_squad;


        // ----- 3.2 Дисперсия шума (EMA от квадрата второй разности) -----

        // hf[n] = x[n] - 2*x[n-1] + x[n-2].
        // Для белого шума дисперсия второй разности равна 6 * sigma_noise^2,
        // поэтому делим на 6 и получаем сразу мощность шума

        float high_frequency_part =
            curr_x - 2.0f * filter_data->prev_value_1 + filter_data->prev_value_2;

        filter_data->prev_value_2 = filter_data->prev_value_1;
        filter_data->prev_value_1 = curr_x;


        float noise_sigma_squad = filter_data->noise_sigma_squad;

        noise_sigma_squad +=
            filter_data->noise_betha *
            ((high_frequency_part * high_frequency_part) / 6.0f - noise_sigma_squad);

        if (!isfinite(noise_sigma_squad) || noise_sigma_squad < 0.0f) noise_sigma_squad = 0.0f;

        filter_data->noise_sigma_squad = noise_sigma_squad;


        float noise_sigma = sqrtf(noise_sigma_squad);


        if (filter_data->filter_warmup_counter < FILTER_WARMUP_SAMPLES)
        {
            filter_data->filter_warmup_counter += 1;
        }


        if (TEST_MODE_2) {

            printf("Новая sigma^2: %lf\n", (double)filter_data->running_sigma_squad);
            printf("Новая noise_sigma: %lf\n", (double)noise_sigma);

        }

        // =========================================================
        // 4. DYNAMIC TRESHOLD (Расчет зоны неопределенности шума)
        // =========================================================

        // Базовый порог - правило k сигм по ШУМУ.
        //
        // Дальше зажимаем его в коридор от MIN_TRESHOLD_PART_OF_AMPLITUDE
        // до MAX_TRESHOLD_PART_OF_AMPLITUDE от текущей амплитуды:
        //
        //  - нижняя граница не даёт порогу схлопнуться в ноль на идеально
        //    чистом сигнале, где шума нет вообще и любая цифровая рябь
        //    порождала бы ложные переходы;
        //
        //  - верхняя граница не даёт порогу съесть саму волну, когда
        //    оценка шума на переходном процессе временно взлетает

        float current_amplitude = peaks_data->running_amplitude;

        if (!isfinite(current_amplitude) || current_amplitude <= 0.0f)
        {
            // Амплитуда ещё не измерена - грубо оцениваем размах по текущей точке
            current_amplitude = 2.0f * fabsf(diff) + ZC_TRESHOLD_START_VALUE;
        }


        float treshold = filter_data->k_treshold * noise_sigma;

        float min_treshold = MIN_TRESHOLD_PART_OF_AMPLITUDE * current_amplitude;
        float max_treshold = MAX_TRESHOLD_PART_OF_AMPLITUDE * current_amplitude;

        if (treshold < min_treshold) treshold = min_treshold;
        if (treshold > max_treshold) treshold = max_treshold;

        filter_data->running_treshold = treshold;



        if (TEST_MODE_2) {

            printf("Новый treshold: %lf\n", (double)filter_data->running_treshold);

        }

        // =========================================================
        // 5. PEAK-детектор на каждый шаг
        // =========================================================
        // Анализируем пики
        // и детектируем полуволны
        runtime_detect_trends(used_scope, curr_idx, prev_idx);
    }
}


void runtime_detect_trends(Scope* used_scope, int curr_idx, int prev_idx)
{

    if (TEST_MODE_3) {

        printf("\n\nЗаход в детектор пиков!\n");
        
    }


    // Только что обновленный буффер
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;


    if (buffer->count < 2) return;

    // Чекаем пики, выставляем всю дату в контроллере пиков
    scope_realtime_peaks_ctx* peaks_data = &used_scope->signal_control_data.peaks_ctx; 

    scope_running_signal_data_ctx* running_data = &used_scope->signal_control_data.running_signal_characteristics;

    scope_realtime_filtering_ctx* filter = &used_scope->signal_control_data.filter_ctx;


    // =========================================================
    // 0. RAW SIGNAL
    // =========================================================

    // Сигнал уже записан, head сдвинут, соответственно

    // Сэмплы сигнала
    sample_t curr = buffer->samples[curr_idx];
    sample_t prev = buffer->samples[prev_idx];

    float curr_x = curr.value;
    double curr_t = curr.time;

    float prev_x = prev.value;
    double prev_t = prev.time;


    if (TEST_MODE_3) {

        printf("\n Curr in detector: %f\n", curr_x);
        printf("Prev in detector: %f\n", prev_x);
    }



    // =========================================================
    // 0. TREND ANALYSIS
    // =========================================================

    trend_type curr_trend;
    trend_type prev_trend;

    // Last trend (Static at init point)

    prev_trend = peaks_data->prev_trend;

    // Current trend

    // Мёртвая зона определения тренда теперь привязана к ШУМУ, а не к полной
    // дисперсии сигнала. Раньше было 0.0005 * sigma, где sigma - разброс всего
    // сигнала: для чистой синусоиды это давало зону в микровольты, то есть
    // тренд переключался буквально на каждой шумовой ряби, и trend_confidence
    // никогда не набиралась

    float noise_sigma = sqrtf(filter->noise_sigma_squad);

    float dx = curr_x - prev_x;
    float trend_eps = 0.25f * noise_sigma;

    if ((fabsf(dx) < trend_eps)) curr_trend = STATIC_PT;
    else if (dx > 0.0f) curr_trend = RISING_PT;
    else curr_trend = FALLING_PT;

    if (TEST_MODE_3) {

        printf("Новый dx: %lf\n", (double)dx);
        
    }

    // =========================================================
    // 1. PEAKS FIXATION
    // =========================================================

    bool peak_trend_status = false;
    bool trough_trend_status = false;
    bool trend_continuation_status = false;


    if (TEST_MODE_3)
    {
        printf("\n\nTRANDS: %d %d\n", prev_trend, curr_trend);
    }

    // Яма
    if (prev_trend == FALLING_PT && curr_trend == RISING_PT) trough_trend_status = true;

    // Пик
    else if (prev_trend == RISING_PT && curr_trend == FALLING_PT) peak_trend_status = true;

    // Статичный тренд (2 подъема подряд, 2 опуска подряд, 2 равенства подряд)
    if (prev_trend == curr_trend) trend_continuation_status = true;


    if (TEST_MODE_3) {

        if (peak_trend_status)
        {
            printf("\n\n=== PEAK ===\n");
            printf("prev_x = %f\n", prev_x);
        }

        if (trough_trend_status)
        {
            printf("\n\n=== TROUGH ===\n");
            printf("prev_x = %f\n", prev_x);
        }

        if (trend_continuation_status)
        {
            printf("\n\n=== CONTINUATION ===\n");
            printf("prev_x = %f\n", prev_x);
        }
    }

    // Проверка доверия к событию:

    // Если одинаково подряд => confidence растёт
    // Если туда-сюда => он колеблется около нуля
    // Если хаос / шум => он разрушается

    if (trend_continuation_status) peaks_data->trend_confidence += 1.0f;
    else peaks_data->trend_confidence -= 1.0f;


    // Защита от завалов уверенности
    if (peaks_data->trend_confidence >= 100.0f) peaks_data->trend_confidence = 100.0f;
    else if (peaks_data->trend_confidence <= 0.0f) peaks_data->trend_confidence = 0.0f;


    if (TEST_MODE_3) {

        printf("Новый trend-confidence: %lf\n", (double)peaks_data->trend_confidence);
        
    }


    // ===== Чек пиков и ям, установка минимумов и максимумов =====

    /*

        Пик №1 найден
                │
                ▼
        peak_candidate = пик №1

        Пик №2 найден
                │
                ▼
        last_peak = пик №1
        peak_candidate = пик №2

                │
                ▼
        current_max = max(пик №1, пик №2)
    
    */


    if (TEST_MODE_3) {

        printf("\n\n Чек экстремумов. Текущие значения: \n");

        printf("running_max = %f\n", peaks_data->running_max);
        printf("max_candidate = %f\n", peaks_data->max_candidate);
        
        printf("running_min = %f\n", peaks_data->running_min);
        printf("min_candidate = %f\n", peaks_data->min_candidate);

        printf("Running amplutude = %f\n", peaks_data->running_amplitude);
        
    }


    // Новое значение - новый пик
    if (peak_trend_status)
    {
        // 1st step error handle
        if (peaks_data->running_max == -FLT_MAX) peaks_data->running_max = curr_x;


        // Прошлый кандидат становится пиком
        if (peaks_data->last_peak == -FLT_MAX) peaks_data->last_peak = curr_x;
        else peaks_data->last_peak = peaks_data->peak_candidate;

        // Прошлый пик становится кандидатом
        if (peaks_data->peak_candidate == -FLT_MAX) peaks_data->peak_candidate = curr_x;
        else peaks_data->peak_candidate = prev_x; 


        // Запоминаем устойчивость тренда
        // в момент фиксации экстремума.
        peaks_data->last_event_confidence = peaks_data->trend_confidence;

        // ===== Чек максимума =====

        float current_max = peaks_data->last_peak > peaks_data->peak_candidate ? peaks_data->last_peak : peaks_data->peak_candidate;

        // Обновляем running_max только если
        // новый максимум получен при более устойчивом тренде.
        bool curr_max_confidence_status = (peaks_data->trend_confidence > 50.0f); 

        if ((current_max >= peaks_data->max_candidate) && curr_max_confidence_status)
        {   
            // Прошлый кандидат становится максимумом
            peaks_data->running_max = peaks_data->max_candidate;


            if (peaks_data->running_max != -FLT_MAX &&
                peaks_data->running_min != FLT_MAX)
            {
                peaks_data->running_amplitude =
                    fabsf(peaks_data->running_max - peaks_data->running_min);
            }

            // Текущий пик становится кандидатом
            peaks_data->max_candidate = current_max;

            peaks_data->max_confidence = peaks_data->trend_confidence;


            if (TEST_MODE_3) {

                printf("running_max = %f\n", peaks_data->running_max);
                printf("max_candidate = %f\n", peaks_data->max_candidate);
                printf("Running amplutude = %f\n", peaks_data->running_amplitude);
                
            }
        }
    } 

    // Новое значение - новая яма
    else if (trough_trend_status)
    {
        // 1st step error handle
        if (peaks_data->running_min == FLT_MAX) peaks_data->running_min = curr_x;
        
        // Прошлый кандидат становится ямой или текущее значение становится ямой на 1 шаге
        if (peaks_data->last_trough == FLT_MAX) peaks_data->last_trough = curr_x;
        else peaks_data->last_trough = peaks_data->trough_candidate;

        // Прошлый пик становится кандидатом или текущее значение становится кандидатом на 1 шаге
        if (peaks_data->trough_candidate == FLT_MAX) peaks_data->trough_candidate = curr_x;
        else peaks_data->trough_candidate  = prev_x;


        // Запоминаем устойчивость тренда
        // в момент фиксации экстремума.
        peaks_data->last_event_confidence = peaks_data->trend_confidence;


        // ===== Чек минимума =====

        float current_min = 

            peaks_data->last_trough < peaks_data->trough_candidate ?
            peaks_data->last_trough :
            peaks_data->trough_candidate;


        // Обновляем running_min только если
        // новый минимум получен при более устойчивом тренде.
        bool curr_min_confidence_status = (peaks_data->trend_confidence > 50.0f); 

        if ((current_min <= peaks_data->min_candidate) && curr_min_confidence_status)
        {   
            peaks_data->running_min = peaks_data->min_candidate;
            peaks_data->min_candidate = current_min;


            if (peaks_data->running_max != -FLT_MAX &&
                peaks_data->running_min != FLT_MAX)
            {
                peaks_data->running_amplitude =
                    fabsf(peaks_data->running_max - peaks_data->running_min);
            }


            peaks_data->min_confidence = peaks_data->trend_confidence;


            if (TEST_MODE_3) {

                printf("running_min = %f\n", peaks_data->running_min);
                printf("min_candidate = %f\n", peaks_data->min_candidate);
                printf("Running amplutude = %f\n", peaks_data->running_amplitude);
                
            }
        }
    } 

    
    // ===== Чек пиков и ям, установка минимумов и максимумов =====


    // =========================================================
    // 3. Zero-cross detection with halfwaves detector 
    // state-machine / halfwaves detector ctx feedback
    // =========================================================

    // Проверка адекватности поступившего значения для чека zero-cross
    
    float deviation = fabsf(curr_x - running_data->running_dc_offset);

    // 0.1σ → почти гарантированный шум
    // 0.5σ → слабый сигнал, но уже интересный
    // 1.0σ → вероятно реальное отклонение
    // 2–3σ → почти точно событие


    // Статус zero-cross перерехода сигнала.
    //
    // Сравнение идёт с ШУМОВОЙ сигмой. Раньше здесь стояла сигма полного
    // сигнала (~0.7 * A для синуса), и порог "не шум" получался равным
    // 0.35 * A - то есть в зону "шум" попадала вся окрестность нуля, где как
    // раз и живут zero-cross-ы. Детектор полуволн получал данные только
    // с вершин волны и по ним пытался ловить переходы через центр
    bool not_noise = (deviation > 0.5f * noise_sigma);

    if (TEST_MODE_3) {

        printf("Новый deviation: %lf\n", (double)deviation);
        
    }


    // Исходя из:
    //
    //  1) текущего характера тренда - curr_trend
    //  2) статуса по проверке сигнала на шум - not_noise
    //  3) текущего состояния state-машины halfwaves detector
    //  4) текущего контекста halfwaves detector
    //
    // Необходимо детектировать текущие переходы через zero-cross
    // по обозначенной методике.
    //
    // В промежуточном состоянии (между zero-cross) необходимо
    // детектировать и аккумулировать скорость изменения сигнала и
    // площадь полуволны. При шумовых возвратах через dc_offset +- tresh нужно
    // сбрасывать counter-ы и полученные значения, чтобы в характеристики полуволны
    // включались только полезные значения.
    //
    // Промежуточным итогом работы данной секции является поступление в 
    // в wave_pattern_detector_ctx готовой полуволны

    // Если текущее значение не шум и у нас есть достаточно данных для начала анализа - производим действия
    // TODO: нужно ли ограничение buffer->count > 1024???

    if (TEST_MODE_3) {

        printf("Решаем детектировать ли пик!\n");

        printf("buffer count = %d\n", buffer->count);
        printf("warmup = %d\n", FILTER_WARMUP_SAMPLES);
        printf("not_noise = %d\n", not_noise);
        
    }


    if (not_noise && buffer->count > FILTER_WARMUP_SAMPLES)
    {

        if (TEST_MODE_4) {

            printf("Значение - не шум. ДЕТЕКТИМ ZC И ОБНОВЛЯЕМ ПОЛУВОЛНЫ!\n");
            printf("Новое значение в ZC анализ: %lf\n", (double)curr_x);

        }
        
        // Helper-функция по детекции zero-cross и заполнению буффера
        runtime_detect_halfwaves(used_scope, curr_x, curr_t, curr_trend);

    }

    peaks_data->prev_trend = curr_trend;
}


void runtime_detect_halfwaves(Scope* used_scope, float current_value, double current_time, trend_type current_trend)
{
    wave_pattern_detector_former_ctx* wpdf_ctx = &used_scope->signal_control_data.wave_pattern_detector_former_data;

    wave_pattern_detector_ctx* wpd_ctx = &used_scope->signal_control_data.wave_pattern_detector_data;

    scope_realtime_filtering_ctx* filter = &used_scope->signal_control_data.filter_ctx;


    signal_position* prev_signal_position = &wpdf_ctx->prev_signal_position;
    wave_pattern_buffer_former_states* prev_buffer_former_state = &wpdf_ctx->prev_buffer_former_state;

    wave_pattern_buffer_former_states* curr_waited_state = &wpdf_ctx->buffer_former_state;

    float curr_dc_offset = used_scope->signal_control_data.running_signal_characteristics.running_dc_offset;
    float curr_treshold = filter->running_treshold;


    // ===== Значения для чистки =====

    float deviation = fabsf(current_value - curr_dc_offset);

    // 0.1σ → почти гарантированный шум
    // 0.5σ → слабый сигнал, но уже интересный
    // 1.0σ → вероятно реальное отклонение
    // 2–3σ → почти точно событие
    
    // Ширина шумовой "подушки" вокруг границ зоны гистерезиса.
    // Считается от ШУМОВОЙ сигмы: раньше бралась сигма полного сигнала,
    // и подушка получалась шире самой волны
    float noise_sigma = sqrtf(filter->noise_sigma_squad);
    float noise_value = 0.5f * noise_sigma;


    float zc_p = curr_dc_offset + curr_treshold;
    float zc_m = curr_dc_offset - curr_treshold;

    float noised_zc_mm = zc_m - noise_value;
    float noised_zc_mp = zc_m + noise_value;

    float noised_zc_pm = zc_p - noise_value;
    float noised_zc_pp = zc_p + noise_value;


    if (TEST_MODE_4)
    {
        printf(

            "\n\nDATA x= %8.4f  dc= %8.4f  thr= %8.4f  "
            "zc- =%8.4f  zc+ =%8.4f  "
            "nz_mm =%8.4f  nz_mp =%8.4f  "
            "nz_pm =%8.4f  nz_pp =%8.4f\n\n\n",
            current_value,
            curr_dc_offset,
            curr_treshold,
            zc_m,
            zc_p,
            noised_zc_mm,
            noised_zc_mp,
            noised_zc_pm,
            noised_zc_pp
        );
    }


    // ===== Значения для чистки =====


    // При первичном оправдании ожидаемого в wave_pattern_buffer_former_states
    // стейта выставляет point_1_time и при посылке сигнала на отсутствие 
    // значимых переходов делает += 1. Если дальнейший значимый переход не оправдывает надежд
    // из wave_pattern_buffer_former_states - point_1_time сбрасывается (перезаписывается на следующем значимом переходе), а 
    // тип значимого перехода меняется на предыдущий по стейт машине. Если надежды
    // оправданы, то записывается point_2_time для текущего перехода, по point_1_time и point_2_time, как  
    // 0.5 (point_1_time + point_2_time) обновляется halfwave_zero_crosses[n] (в зависимости от типа перехода n = 0 или 1)


    // ===== State machine main flags for this step =====

    signal_position curr_signal_position;

    // Rising ZC
    if (
        
        (current_value <= (curr_dc_offset + curr_treshold)) && 
        (current_value >= (curr_dc_offset - curr_treshold))
    
    ) curr_signal_position = INSIDE_ZC_TZ_SP;

    else if (current_value < (curr_dc_offset - curr_treshold)) curr_signal_position = BELOW_ZC_TZ_SP;

    else curr_signal_position = ABOVE_ZC_TZ_SP;


    // После перезагрузки при попадании в зону начинаем работу с первого восходящего через dc_offset
    if (*curr_waited_state == LIMIT_FS && curr_signal_position == BELOW_ZC_TZ_SP) 
    {
        *curr_waited_state = WAIT_RISING_MINUS_FS;
    }

    // Пропустить дальнейшее выполнение логики функции в цикле, если сигнал не вышел в зону инициации
    if (*curr_waited_state == LIMIT_FS) return;

    // ===== State machine main flags for this step =====
    

    // ===== State machine step =====

    // В случае дефолтной позиции текущего значения сигнала (не переход через dc_offfset)
    // производим аккумуляцию текущих значений в контексте
    
    // В случае захода и дропа производим сброс значений времени

    // В некоторых случаях приход флага noised ведёт к выставлению noised_flag в true, что управляет
    // логикой аккумуляции данных о полуволне 

    // State-machine
    switch (*curr_waited_state)
    {
        // ===== CASE 1 =====

        // Ожидаем проход через нижнюю границу трешхолд зоны
        case WAIT_RISING_MINUS_FS:

            // При подобном ожидании полуволна автоматом - RISING
            wpdf_ctx->curr_halfwave.halfwave_type = RISING_PT;


            
            // 1 случай - сигнал ниже treshold zone
            if (curr_signal_position == BELOW_ZC_TZ_SP)
            {
                // 1.1 случай - пришли в зону из одной из прошлых зон
                // TODO: чек надо ли вообще сравниваться с ABOVE
                if (*prev_signal_position == ABOVE_ZC_TZ_SP || *prev_signal_position == INSIDE_ZC_TZ_SP)
                {
                    // Если не было блока нарастаний (который выставляется при реализации
                    // первого перехода и возврате к пред. стейту до реализации второго)
                    if (!wpdf_ctx->accumulation_block)
                    {
                        // Для того, чтобы отделить нормальный переход от того, который
                        // мог быть из-за шума в начале или конце зоны
                        // Делаем чек на "шумность" относительно нужного трешхолда
                        if (current_value <= noised_zc_mm)
                        {
                            // Мы перешли и прошли зону шума
                            // Пишем точку
                            halfwaves_detector_accumulation(used_scope, current_value, current_time);

                            // Меняем предыдущую на текущею при завершении прохода
                            *prev_signal_position = curr_signal_position;
                        }
                    }
                }

                // 1.2 случай - пришли в зону из такой же зоны
                if (*prev_signal_position == BELOW_ZC_TZ_SP)
                {
                    // Копим дату, обновляем параметры +
                    // Делаем запись последней нормальной точки,
                    // В случае, если не был введен блок аккумуляции 
                    if (!wpdf_ctx->accumulation_block)
                    {
                        if (current_value <= noised_zc_mm)
                        {
                            // Пишем точку
                            halfwaves_detector_accumulation(used_scope, current_value, current_time);

                            // Меняем предыдущую на текущею при завершении прохода
                            *prev_signal_position = curr_signal_position;
                        }
                    }
                }
            }

            // Перешли в зону трешхолда - выставляем блок обновлений 
            // и ждём нарастающий сигнал
            else if (curr_signal_position == INSIDE_ZC_TZ_SP)
            {
                // ИСПРАВЛЕНО: условие было инвертировано - стояло
                // !(current_value >= noised_zc_mp), то есть переход засчитывался
                // только если сигнал, войдя в зону снизу, остался НИЖЕ
                // (zc_m + шум). Для сигнала, который реально идёт вверх,
                // это условие почти никогда не выполнялось, и стейт-машина
                // застревала в CASE 1
                if (current_value >= noised_zc_mp)
                {
                    // Ставим блок на аккумуляцию последующего прихода значений
                    // по последнему halfwaves_detector_accumulation имеем в памяти:

                        // float prev_clean_signal_value;          // Значение предыдущего сигнала с допустимым доверием
                        // float prev_clean_signal_time;           // Время предыдущего сигнала с допустимым доверием

                    wpdf_ctx->accumulation_block = true;

                    // Меняем ожидание на проход через верхнюю границу трешхолд зоны
                    // пока она не будет пройдена будет сохраняться блок аккумуляции и 2 
                    // последних чистых значения сигнала и времени
                
                    *curr_waited_state = WAIT_RISING_PLUS_FS;

                    // Меняем предыдущую позицию сигнала на текущею при завершении прохода
                    *prev_signal_position = curr_signal_position;
                }
            }


            // Прошли сразу обе зоны за 1 степ -
            // делаем чек на шум, считаем чистый zero cross
            // и отправляем его в контекст чистых переходов 
            else if (curr_signal_position == ABOVE_ZC_TZ_SP)
            {
                // ИСПРАВЛЕНО: условие было инвертировано - стояло
                // !(current_value >= noised_zc_pp). Получалось, что переход
                // засчитывался только если сигнал вылез над верхней границей,
                // но НЕ выше неё плюс шум, то есть попал в узкую полоску.
                // Быстрый фронт (меандр, высокая частота) эту полоску
                // перепрыгивает за один сэмпл, полуволна не закрывалась,
                // и детектор периода просто переставал получать данные
                if (current_value >= noised_zc_pp)
                {
                    // Знаем обе крайние точки - это текущая и последняя записанная, как нормальная в:

                        // float prev_clean_signal_value;          // Значение предыдущего сигнала с допустимым доверием
                        // float prev_clean_signal_time;           // Время предыдущего сигнала с допустимым доверием



                    // Производим дроп значений с отправкой текущей даты по полуволне
                    drop_zc_accumulation(used_scope, current_value, current_time);


                    // Снимаем блок на аккумуляцию последующего прихода значений
                    wpdf_ctx->accumulation_block = false;

                    // Меняем ожидание на проход через верхнюю границу трешхолд зоны
                    *curr_waited_state = WAIT_FALLING_PLUS_FS;

                    // Меняем предыдущую позицию сигнала на текущею при завершении прохода
                    *prev_signal_position = curr_signal_position;
                }
            }

            break;

        // ===== CASE 1 =====


        // ===== CASE 2 =====

        case WAIT_RISING_PLUS_FS:

            // Нет смены типа полуволны -> wpdf_ctx->curr_halfwave.halfwave_type == RISING_PT;

            // требуемые блоки были выставлены в CASE 1.
            // в CASE 2 только ожидаем прохода над ABOVE_ZC_TZ_SP
            // репитим логику с CASE 1 для такого же прохода

            // Нет смысла делать чек чего-то кроме выхода за верхнюю границу

            if (curr_signal_position == ABOVE_ZC_TZ_SP)
            {
                // ИСПРАВЛЕНО: то же инвертированное условие, что и в CASE 1.
                // Теперь восходящий переход засчитывается, когда сигнал
                // ГАРАНТИРОВАННО ушёл выше верхней границы плюс шум
                if (current_value >= noised_zc_pp)
                {
                    // Знаем обе крайние точки - это текущая и последняя записанная, как нормальная в:

                        // float prev_clean_signal_value;          // Значение предыдущего сигнала с допустимым доверием
                        // float prev_clean_signal_time;           // Время предыдущего сигнала с допустимым доверием



                    // Производим дроп значений с отправкой текущей даты по полуволне
                    drop_zc_accumulation(used_scope, current_value, current_time);


                    // Снимаем блок на аккумуляцию последующего прихода значений
                    wpdf_ctx->accumulation_block = false;

                    // Меняем ожидание на проход через верхнюю границу трешхолд зоны
                    *curr_waited_state = WAIT_FALLING_PLUS_FS;

                    // Меняем предыдущую позицию сигнала на текущею при завершении прохода
                    *prev_signal_position = curr_signal_position;
                }
            }
        
            break;

        // ===== CASE 2 =====


        // 3 и 4 кейс - инверсии относительно 1 и 2


        // ===== CASE 3 =====

        case WAIT_FALLING_PLUS_FS:

            // При подобном ожидании полуволна автоматом - FALLING
            wpdf_ctx->curr_halfwave.halfwave_type = FALLING_PT;
            
            // 1 случай
            if (curr_signal_position == ABOVE_ZC_TZ_SP)
            {
                // 1.1 случай - пришли в зону из одной из прошлых зон
                // TODO: чек надо ли вообще сравниваться с ABOVE
                if (*prev_signal_position == BELOW_ZC_TZ_SP || *prev_signal_position == INSIDE_ZC_TZ_SP)
                {
                    // Если не было блока нарастаний (который выставляется при реализации
                    // первого перехода и возврате к пред. стейту до реализации второго)
                    if (!wpdf_ctx->accumulation_block)
                    {
                        // Для того, чтобы отделить нормальный переход от того, который
                        // мог быть из-за шума в начале или конце зоны
                        // Делаем чек на "шумность" относительно нужного трешхолда
                        if (current_value >= noised_zc_pp)
                        {
                            // Мы перешли и прошли зону шума
                            // Пишем точку
                            halfwaves_detector_accumulation(used_scope, current_value, current_time);

                            // Меняем предыдущую на текущею при завершении прохода
                            *prev_signal_position = curr_signal_position;
                        }
                    }
                }

                // 1.2 случай - пришли в зону из такой же зоны
                if (*prev_signal_position == ABOVE_ZC_TZ_SP)
                {
                    // Копим дату, обновляем параметры +
                    // Делаем запись последней нормальной точки,
                    // В случае, если не был введен блок аккумуляции 
                    if (!wpdf_ctx->accumulation_block)
                    {
                        if (current_value >= noised_zc_pp)
                        {
                            // Пишем точку
                            halfwaves_detector_accumulation(used_scope, current_value, current_time);

                            // Меняем предыдущую на текущею при завершении прохода
                            *prev_signal_position = curr_signal_position;
                        }
                    }
                }
            }

            // 2 случай - Перешли в зону трешхолда - выставляем блок обновлений 
            // и ждём нарастающий сигнал
            else if (curr_signal_position == INSIDE_ZC_TZ_SP)
            {
                // Выставляем блок при нормальном переходе с учетом шума
                if (current_value <= noised_zc_pm)
                {
                    // Ставим блок на аккумуляцию последующего прихода значений
                    // по последнему halfwaves_detector_accumulation имеем в памяти:

                        // float prev_clean_signal_value;          // Значение предыдущего сигнала с допустимым доверием
                        // float prev_clean_signal_time;           // Время предыдущего сигнала с допустимым доверием

                    wpdf_ctx->accumulation_block = true;

                    // Меняем ожидание на проход через верхнюю границу трешхолд зоны
                    // пока она не будет пройдена будет сохраняться блок аккумуляции и 2 
                    // последних чистых значения сигнала и времени
                
                    *curr_waited_state = WAIT_FALLING_MINUS_FS;

                    // Меняем предыдущую позицию сигнала на текущею при завершении прохода
                    *prev_signal_position = curr_signal_position;
                }
            }

            // 3 случай - Прошли сразу обе зоны за 1 степ -
            // делаем чек на шум, считаем чистый zero cross
            // и отправляем его в контекст чистых переходов
            if (curr_signal_position == BELOW_ZC_TZ_SP)
            {
                // ИСПРАВЛЕНО: проверка шла относительно ВЕРХНЕЙ границы
                // (noised_zc_pm), хотя проверяется уход под НИЖНЮЮ.
                // Условие выполнялось всегда, и нисходящий переход
                // засчитывался вообще без шумовой защиты - асимметрично
                // восходящему
                if (current_value <= noised_zc_mm)
                {
                    // Знаем обе крайние точки - это текущая и последняя записанная, как нормальная в:
                        
                        // float prev_clean_signal_value;          // Значение предыдущего сигнала с допустимым доверием
                        // float prev_clean_signal_time;           // Время предыдущего сигнала с допустимым доверием


                    // Производим дроп значений с отправкой текущей даты по полуволне
                    drop_zc_accumulation(used_scope, current_value, current_time);


                    // Снимаем блок на аккумуляцию последующего прихода значений
                    wpdf_ctx->accumulation_block = false;

                    // Меняем ожидание на проход через верхнюю границу трешхолд зоны
                    *curr_waited_state = WAIT_RISING_MINUS_FS;

                    // Меняем предыдущую позицию сигнала на текущею при завершении прохода
                    *prev_signal_position = curr_signal_position;
                }
            }
        
            break;

        // ===== CASE 3 =====


        // ===== CASE 4 =====

        case WAIT_FALLING_MINUS_FS:
            
            // Нет смены типа полуволны -> wpdf_ctx->curr_halfwave.halfwave_type == FALLING_PT;

            // требуемые блоки были выставлены в CASE 3.
            // в CASE 4 только ожидаем прохода над BELOW_ZC_TZ_SP
            // репитим логику с CASE 3 для такого же прохода

            if (curr_signal_position == BELOW_ZC_TZ_SP)
            {
                // ИСПРАВЛЕНО: та же подмена границы, что и в CASE 3
                if (current_value <= noised_zc_mm)
                {
                    // Знаем обе крайние точки - это текущая и последняя записанная, как нормальная в:
                        
                        // float prev_clean_signal_value;          // Значение предыдущего сигнала с допустимым доверием
                        // float prev_clean_signal_time;           // Время предыдущего сигнала с допустимым доверием



                    // Производим дроп значений с отправкой текущей даты по полуволне
                    drop_zc_accumulation(used_scope, current_value, current_time);


                    // Снимаем блок на аккумуляцию последующего прихода значений
                    wpdf_ctx->accumulation_block = false;

                    // Меняем ожидание на проход через верхнюю границу трешхолд зоны
                    *curr_waited_state = WAIT_RISING_MINUS_FS;

                    // Меняем предыдущую позицию сигнала на текущею при завершении прохода
                    *prev_signal_position = curr_signal_position;
                }
            }
        
            break;

        // ===== CASE 4 =====


        default: break;
    }

    // ===== State machine step =====

}



void halfwaves_detector_accumulation(Scope* used_scope, float current_value, double current_time)
{

    if (TEST_MODE_5) {

        printf("Аккумуляция детектор! в ZC анализ: %lf\n", (double)current_value);
        
    }


    // Accumulate by curr and prev
    wave_pattern_detector_former_ctx* wpdf_ctx = &used_scope->signal_control_data.wave_pattern_detector_former_data;

    wave_pattern_detector_ctx* wpd_ctx = &used_scope->signal_control_data.wave_pattern_detector_data;



    float curr_x = current_value;
    float prev_x = wpdf_ctx->prev_clean_signal_value;

    double curr_time = current_time;
    double prev_time = wpdf_ctx->prev_clean_signal_time;


    // ===== Accumulation logic =====

    // Area

    /*

        ИСПРАВЛЕНО: раньше площадь копилась как halfwave_area += curr_x,
        то есть как сумма СЫРЫХ значений сигнала.

        Такая "площадь" ломалась сразу по двум причинам:

            1) она зависела от постоянной составляющей - при dc_offset 1 В
               площадь полуволны длиной 100 сэмплов получала 100 В "бесплатно";

            2) она зависела от того, сколько сэмплов детектор реально принял.
               Шум заставляет пропускать точки, поэтому две ФИЗИЧЕСКИ одинаковые
               полуволны получали разную площадь, и метрика halfwave_distance
               объявляла их разными элементами паттерна.

        Теперь это честный интеграл отклонения от центра сигнала по времени:

            S = SUM (x - dc) * dt

        Величина имеет размерность В*с, не зависит ни от смещения нуля,
        ни от числа принятых точек.

    */

    float curr_dc_offset = used_scope->signal_control_data.running_signal_characteristics.running_dc_offset;

    const double MIN_DT = 1.0 / SCOPE_SAMPLE_RATE;

    double dt = fabs(curr_time - prev_time);

    // Защита от нулевого и от абсурдно большого шага
    // (первая точка полуволны, разрыв данных после включения-выключения)
    if (dt < MIN_DT || dt > 1.0) dt = MIN_DT;


    wpdf_ctx->curr_halfwave.halfwave_area += (float)((curr_x - curr_dc_offset) * dt);

    wpdf_ctx->curr_halfwave.samples_in_halfwave += 1;


    // Peaks with protection

    if (wpdf_ctx->curr_halfwave.peak_value != -FLT_MAX)
    {
        wpdf_ctx->curr_halfwave.peak_value = fmaxf(wpdf_ctx->curr_halfwave.peak_value, curr_x);
    }
    else wpdf_ctx->curr_halfwave.peak_value = curr_x;


    // ИСПРАВЛЕНО: здесь проверялось peak_value != FLT_MAX вместо
    // trough_value != FLT_MAX. Копипаста из блока выше
    if (wpdf_ctx->curr_halfwave.trough_value != FLT_MAX)
    {
        wpdf_ctx->curr_halfwave.trough_value = fminf(wpdf_ctx->curr_halfwave.trough_value, curr_x);
    }
    else wpdf_ctx->curr_halfwave.trough_value = curr_x;


    // Обновление чистых значений после последней аккумуляции, 
    // чтобы при следующей аккумуляции использовать их
    // для расчёта скорости и площади полуволны

    wpdf_ctx->prev_clean_signal_value = curr_x;
    wpdf_ctx->prev_clean_signal_time = curr_time;
}


// Helper-states for drop_zc_accumulation
typedef enum zero_cross_fill_status
{
    
    TWO_ZC_EMPTY,       // Только в случае инициализации
    FIRST_ZC_FILLED,    // Первый переход
    SECOND_ZC_FILLED    // Второй переход - в конце сдвигается и становится первым

} zero_cross_fill_status;



void drop_zc_accumulation(Scope* used_scope, float current_value, double current_time)
{

    if (TEST_MODE_5) {

        printf("Дроп детектора!: %lf\n", (double)current_value);
        
    }


    wave_pattern_detector_former_ctx* wpdf_ctx = &used_scope->signal_control_data.wave_pattern_detector_former_data;
    
    halfwave_data_ctx* curr_halfwave = &wpdf_ctx->curr_halfwave;


    // ===== Чек статуса для выбора логики дропа значений =====

    // Инициализация обязательна: при обоих заполненных zero-cross ни одна
    // из веток ниже не срабатывала, и дальше читалась неинициализированная
    // переменная. Логика такого состояния не допускает, но полагаться
    // на это в C нельзя - это чистое неопределённое поведение
    zero_cross_fill_status curr_zc_status = SECOND_ZC_FILLED;

    if (!wpdf_ctx->halfwave_zero_crosses[0].filled)
        curr_zc_status = TWO_ZC_EMPTY;

    else if (wpdf_ctx->halfwave_zero_crosses[0].filled && !wpdf_ctx->halfwave_zero_crosses[1].filled )
        curr_zc_status = FIRST_ZC_FILLED;

    // При текущей архитектуре в данной зоне нет нужды производить чек на SECOND_HALFWAVE_FILLED status

    // ===== Чек статуса для выбора логики дропа значений =====


    // ===== Заполнения zero-cross и дроп аккумуляции =====

    float curr_x = current_value;
    float prev_x = wpdf_ctx->prev_clean_signal_value;

    double curr_time = current_time;
    double prev_time = wpdf_ctx->prev_clean_signal_time;


    trend_type curr_trend = curr_halfwave->halfwave_type;



    if (curr_zc_status == TWO_ZC_EMPTY)
    {
        // Заполняем данные о первом zero-cross после инициализации

        // Чекаем предыдущие чистые значения и ищем среднее арифметическое с переданными current


        wpdf_ctx->halfwave_zero_crosses[0].zero_cross_type = curr_trend;
        wpdf_ctx->halfwave_zero_crosses[0].time = (curr_time + prev_time) * 0.5;
        wpdf_ctx->halfwave_zero_crosses[0].filled = true;

        // Простой дроп данных полуволны без передачи - текущая полуволна была, как своеобразный "затакт"
        // и мы всегда будем работать с волнами, которые FALLING -> RISING, вроде таких:

        /*
            Типовая волна с которой мы работаем:

                 _ _ _
                |     |  Falling
                |     |
            ____|_____|_________
                      |     |
                      |     |  Rising
                      |_ _ _|
                    
        */

        

        // Дроп всего (кроме curr_halfwave->halfwave_type - контролируется лишь извне) под 1 полуволну:
        // TODO: Надо ли с учетом INIT?

        curr_halfwave->start_time = 0.0f;
        curr_halfwave->end_time = 0.0f;
        curr_halfwave->halfwave_full_time = 0.0f;
        curr_halfwave->peak_value = -FLT_MAX;
        curr_halfwave->trough_value = FLT_MAX;
        curr_halfwave->halfwave_area = 0.0f;
        curr_halfwave->halfwave_smoothed_speed = 0.0f;
        curr_halfwave->samples_in_halfwave = 0;

    }


    if (curr_zc_status == FIRST_ZC_FILLED)
    {
        // Чекаем предыдущие чистые значения и ищем среднее арифметическое с переданными current

        wpdf_ctx->halfwave_zero_crosses[1].zero_cross_type = curr_trend;
        wpdf_ctx->halfwave_zero_crosses[1].time = (curr_time + prev_time) * 0.5;
        wpdf_ctx->halfwave_zero_crosses[1].filled = true;


        curr_halfwave->start_time = wpdf_ctx->halfwave_zero_crosses[0].time;
        curr_halfwave->end_time = wpdf_ctx->halfwave_zero_crosses[1].time;

        // Разность с защитой от переполнения для нахождения времени полуволны
        if (!isnan(curr_halfwave->end_time) && !isnan(curr_halfwave->start_time))
        {
            // 2. Безопасное вычисление разности (в double, а не в float:
            //    разность двух больших времён во float теряет точность)
            double diff = curr_halfwave->end_time - curr_halfwave->start_time;

            // 3. Проверка на бесконечность
            if (isinf(diff))
            {
                // Обработка ошибки: значение слишком велико
                curr_halfwave->halfwave_full_time = (diff > 0) ? FLT_MAX : -FLT_MAX;
            }
            else
            {
                curr_halfwave->halfwave_full_time = diff;
            }
        }


        // Средняя скорость изменения сигнала внутри полуволны.
        //
        // Считаем один раз, здесь, как размах, делённый на длительность.
        // Раньше это была EMA от мгновенных |dx/dt| с коэффициентом 0.5,
        // то есть значение почти целиком определялось последним сэмплом
        // и от полуволны к полуволне гуляло в разы даже на чистом сигнале -
        // а halfwave_distance принимала это гуляние за разницу формы

        if (curr_halfwave->halfwave_full_time > 0.0 &&
            curr_halfwave->peak_value != -FLT_MAX &&
            curr_halfwave->trough_value != FLT_MAX)
        {
            curr_halfwave->halfwave_smoothed_speed =
                (float)(fabsf(curr_halfwave->peak_value - curr_halfwave->trough_value) /
                        curr_halfwave->halfwave_full_time);
        }
        else curr_halfwave->halfwave_smoothed_speed = 0.0f;
    }


    // ===== Заполнения zero-cross и дроп аккумуляции =====


    // ===== Передача накопившихся о полуволне в случае заполнения 2 zero-cross =====

    if (wpdf_ctx->halfwave_zero_crosses[0].filled && wpdf_ctx->halfwave_zero_crosses[1].filled)
        curr_zc_status = SECOND_ZC_FILLED;

    // Выход если не заполнены оба zero-cross
    else 
    {
        // ===== Конечный сдвиг аккумуляторов при любом заходе в этот блок ===== 

        wpdf_ctx->prev_clean_signal_value = current_value;
        wpdf_ctx->prev_clean_signal_time = current_time;

        return;
    }


    // Обработка при заполнении всех zero-cross
    if (curr_zc_status == SECOND_ZC_FILLED)
    {

        // Передача текущей полуволны в буффер полуволн со сдвигом буффера
        // при помощи helper-функции.
        //
        // При вызове этой функции мы имеем полностью заполненный контекст
        // полуволны с вычисленными на стадии аккумуляции значений:
        //
        //    curr_halfwave->peak_value
        //    curr_halfwave->trough_value
        //    curr_halfwave->halfwave_area
        //    curr_halfwave->halfwave_smoothed_speed

            if (TEST_MODE_5)
            {

                wave_pattern_detector_ctx* wpd_ctx = &used_scope->signal_control_data.wave_pattern_detector_data;

                printf(

            "\nSEND:\n"
                    "peak =%f\n"
                    "trough =%f\n"
                    "time =%f\n"
                    "area =%f\n"
                    "speed =%f\n"
                    "curr halfwaves count = %u\n",
                    curr_halfwave->peak_value,
                    curr_halfwave->trough_value,
                    curr_halfwave->halfwave_full_time,
                    curr_halfwave->halfwave_area,
                    curr_halfwave->halfwave_smoothed_speed,
                    wpd_ctx->count
                );

            }


        add_halfwave_in_buffer(used_scope, wpdf_ctx->curr_halfwave);


        // Полуволна передана - производим очистку аккумуляторов 

        // ===== Сброс аккумуляторов полуволны ===== 

        // Передача времени окончания прошлой как времени старта текущей
        curr_halfwave->start_time = curr_halfwave->end_time;

        // Дроп всего остального

        curr_halfwave->end_time = curr_halfwave->end_time;  // Стал "относительным нулём"
        curr_halfwave->halfwave_full_time = 0.0f;
        curr_halfwave->peak_value = -FLT_MAX;
        curr_halfwave->trough_value = FLT_MAX;
        curr_halfwave->halfwave_area = 0.0f;
        curr_halfwave->halfwave_smoothed_speed = 0.0f;
        curr_halfwave->samples_in_halfwave = 0;


        // ===== Сброс аккумуляторов zero_cross ===== 

        // Сдвигаем второй zero-cross в первый, чтобы освободить место для следующего

        wpdf_ctx->halfwave_zero_crosses[0].zero_cross_type = wpdf_ctx->halfwave_zero_crosses[1].zero_cross_type;
        wpdf_ctx->halfwave_zero_crosses[0].time = wpdf_ctx->halfwave_zero_crosses[1].time;
        wpdf_ctx->halfwave_zero_crosses[0].filled = true;

        // Очистка 2 под запись нового (т.к. filled == false, то на следующем вызове
        // выставится FIRST_ZC_FILLED и циклично пойдет дальнейшая запись полуволн в буффер)
        wpdf_ctx->halfwave_zero_crosses[1].zero_cross_type = STATIC_PT;
        wpdf_ctx->halfwave_zero_crosses[1].time = 0.0f;
        wpdf_ctx->halfwave_zero_crosses[1].filled = false;


        // ===== Конечный сдвиг аккумуляторов при любом заходе в этот блок ===== 

        // Данная точка перехода теперь будет являться последней "чистой точкой"
        wpdf_ctx->prev_clean_signal_value = current_value;
        wpdf_ctx->prev_clean_signal_time = current_time;
    }
}



void add_halfwave_in_buffer(Scope* used_scope, halfwave_data_ctx new_halfwave)
{

    if (TEST_MODE_4) {

        printf("\n\nПолуволна добавляется в буффер!\n\n");
        
    }

    // First step protection
    if (new_halfwave.peak_value == -FLT_MAX || new_halfwave.trough_value == FLT_MAX)
    {
        
        if (TEST_MODE_4) {

            printf("\n\nПопытка неинициализированной передачи!\n\n");
        
        }

        return;
    }



    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;

    // ===== Заполнение буффера =====

    int head = buffer->head;


    buffer->halfwaves_for_detection[head] = new_halfwave;


    // update ring buffer c защитой от кольцевых переходов
    buffer->head = (head + 1) % PERIOD_DETECTOR_BUFFER_SIZE;

    if (buffer->count < PERIOD_DETECTOR_BUFFER_SIZE) buffer->count++;
}


float halfwave_distance(Scope* used_scope, const halfwave_data_ctx* halfwave_1, const halfwave_data_ctx* halfwave_2)
{
    // =========================================================
    // 1. AMPLITUDE DISTANCE
    // =========================================================
    // Сравниваем амплитуды полуволн (peak - trough).
    // Получаем относительную разницу в диапазоне [0;1].

    float amp_1 = fabsf(halfwave_1->peak_value - halfwave_1->trough_value);
    float amp_2 = fabsf(halfwave_2->peak_value - halfwave_2->trough_value);


    float amp_dist =
        fabsf(amp_1 - amp_2) /
        fmaxf(fmaxf(amp_1, amp_2), 1e-6f);


    // =========================================================
    // 2. TIME DISTANCE
    // =========================================================
    // Сравниваем длительность полуволн.
    // Используется относительная ошибка.

    float time_dist =

        fabsf((float)(halfwave_1->halfwave_full_time -
                      halfwave_2->halfwave_full_time)) /

        fmaxf((float)fmax(halfwave_1->halfwave_full_time,
                          halfwave_2->halfwave_full_time), 1e-6f);


    // =========================================================
    // 3. AREA DISTANCE
    // =========================================================
    // Сравниваем площади полуволн.
    // Нормировка делает метрику независимой от масштаба сигнала.
    //
    // Теперь эта нормировка наконец корректна по размерности: после
    // исправления аккумулятора площадь измеряется в В*с, и ожидаемая площадь
    // "половина амплитуды на среднюю длительность" - величина того же порядка.
    // Раньше площадь была безразмерной суммой отсчётов, и деление на В*с
    // давало произвольное число, которое почти всегда упиралось в потолок 1.0


    float expected_area =

        0.5f * (used_scope->signal_control_data.peaks_ctx.running_amplitude)
        * (float)(halfwave_1->halfwave_full_time + halfwave_2->halfwave_full_time) * 0.5f;

    float x =
        fabsf(halfwave_1->halfwave_area - halfwave_2->halfwave_area ) /
        fmaxf(expected_area, 1e-6f);

    float area_dist = x / (1.0f + x);

        
    // =========================================================
    // 4. SPEED DISTANCE
    // =========================================================
    // Сравниваем среднюю скорость изменения сигнала
    // внутри полуволны.

    float speed_dist =

        fabsf(halfwave_1->halfwave_smoothed_speed -
              halfwave_2->halfwave_smoothed_speed) /

        fmaxf(
            fmaxf(fabsf(halfwave_1->halfwave_smoothed_speed),
                  fabsf(halfwave_2->halfwave_smoothed_speed)),
            1e-6f
        );


    // =========================================================
    // 5. LIMIT TO [0;1]
    // =========================================================
    // Любая составляющая расстояния не должна превышать единицу.

    amp_dist   = fminf(1.0f, amp_dist);
    time_dist  = fminf(1.0f, time_dist);
    area_dist  = fminf(1.0f, area_dist);
    speed_dist = fminf(1.0f, speed_dist);
    
    float total = 0.5f * amp_dist
                + 0.4f * time_dist
                + 0.05f * area_dist
                + 0.05f * speed_dist;

    // =========================================================
    // 6. FINAL DISTANCE
    // =========================================================
    // Формируем единую меру различия.
    // Чем ближе результат к нулю, тем более похожи полуволны.
    // Вес амплитуды снижен, так как для распознавания паттернов
    // важнее форма, чем абсолютный уровень сигнала.
    
    /*
    if (TEST_MODE_5)
    {

        printf(
        "\nСравниваем её с: T=%f  A=%f  S=%f\n",

            halfwave_2->halfwave_full_time,
            halfwave_2->halfwave_area,
            halfwave_2->halfwave_smoothed_speed
        );


        printf(

            "\n Результат: amp=%f time=%f area=%f speed=%f total=%f\n\n",
            amp_dist,
            time_dist,
            area_dist,
            speed_dist,
            total

        );
    }
    */

    return total;
}


// scope_slow_update() часть анализа

float normalized_difference(float a, float b)
{
    float denom = fmaxf(fabsf(a), fabsf(b));

    if (denom < 1e-6f)
        return 0.0f;

    return fabsf(a - b) / denom;
}



int compare_float(const void* a, const void* b)
{
    float fa = *(const float*)a;
    float fb = *(const float*)b;

    if (fa < fb) return -1;
    if (fa > fb) return 1;

    return 0;
}


// =========================================================================================== PATTERN AND PERIOD DETECTION


/*

    ЗАЧЕМ ЭТОТ БЛОК ПЕРЕПИСАН

    Старый детектор работал так:

        1) жадно "раскрашивал" полуволны в классы: первая непомеченная волна
           получала новый номер, и все дальнейшие волны, у которых расстояние
           до неё меньше 0.3, получали тот же номер;

        2) для каждой длины L считал долю совпадений pattern[i] == pattern[i + L]
           по УДВОЕННОМУ буферу;

        3) возвращал median_pattern_len из отладочного return, из-за которого
           весь дальнейший выбор между кандидатами был мёртвым кодом.

    Каждый из трёх шагов ломался по своей причине.

        Шаг 1. Классификация зависела от порядка. Одна испорченная шумом
        полуволна получала новый номер, и все похожие на неё дальше по буферу
        тоже уходили в этот новый класс - строка "121212121212" превращалась
        в "12121261212125" не из-за одной ошибки, а из-за лавины.

        Шаг 2. Главная ловушка. Массив был удвоен (ABC -> ABCABC), а сравнение
        шло для всех i от 0 до count - 1 включительно. При L, близком к count,
        pattern[i + L] попадал во ВТОРУЮ копию и фактически сравнивался
        с pattern[i + L - count], то есть сам с собой со сдвигом (L - count).
        Для периода 2 и L = count - 2 это сравнение элемента с элементом
        на две позиции назад, то есть ВСЕГДА совпадение, score = 1.0.
        Алгоритм систематически поощрял огромные L. На чистом сигнале
        побеждал L = 2 просто потому, что тоже давал 1.0 и шёл раньше,
        а на шумном L = 2 давал 0.86, а какой-нибудь L = 7 давал 0.93 -
        и детектор выдавал "7" вместо "2". Ровно та жалоба, что в задаче.

        Шаг 3. Отладочный return.


    ЧТО СДЕЛАНО ВМЕСТО ЭТОГО

    1) Классификация стала не жадной, а по ближайшему классу: полуволна
       присваивается ТОМУ классу, до которого расстояние минимально, а не
       первому подошедшему. Эталон класса при этом мягко подтягивается к
       принятому элементу, поэтому один выброс не сдвигает класс целиком.

    2) Автокорреляция считается только по РЕАЛЬНЫМ парам (i, i + L),
       которые обе лежат внутри данных. Удвоение буфера больше не нужно
       и убрано: проблема "начало периода в произвольной точке кольца"
       решается разворотом кольца по времени, а не копированием.

    3) Свидетелей теперь два, и они независимы:

        - символьная автокорреляция по строке классов;
        - временнАя автокорреляция по длительностям полуволн.

       Вторая вообще не зависит от того, правильно ли отработала
       классификация: даже если раскраска сбилась, длительности всё равно
       повторяются с шагом истинного периода. Итоговая оценка - взвешенная
       сумма. Чтобы победить, кандидат должен убедить обоих свидетелей.

    4) Схлопывание кратностей. Если период L получил оценку S, а какой-то
       его делитель d получил оценку не хуже, чем S - PATTERN_DIVISOR_TOLERANCE,
       то настоящий период - d. Это формальный способ сказать
       "период 4 у сигнала 1212 - это на самом деле период 2",
       и он же добивает остатки завышенных L.

    5) L перебирается только по чётным значениям. У периодического сигнала
       за период происходит одинаковое число переходов вверх и вниз,
       поэтому число полуволн в периоде всегда чётное. Это сразу выкидывает
       из перебора половину заведомо ложных кандидатов, включая "7".

    6) Гейт на приём. Паттерн принимается, только если итоговая оценка
       не ниже PATTERN_MIN_SCORE и целые повторы покрывают не меньше
       PATTERN_MIN_COVERAGE буфера. Если гейт не пройден - период просто
       НЕ обновляется, и на дисплее остаётся прошлое достоверное значение.
       Показать старое верное значение лучше, чем новое случайное.

*/


// Линейная (развёрнутая по времени) копия кольца полуволн.
//
// Статический буфер, а не VLA на стеке: размер фиксирован и известен на
// этапе компиляции, а анализ синхронный и однопоточный, поэтому одного
// рабочего массива хватает даже при нескольких объектах осциллографа
static halfwave_data_ctx linear_halfwaves[PERIOD_DETECTOR_BUFFER_SIZE];

// Номера классов для каждой полуволны линейной копии
static int halfwave_labels[PERIOD_DETECTOR_BUFFER_SIZE];


/**
 * @brief Разворачивает кольцевой буфер полуволн в линейный по возрастанию времени.
 *
 * Кольцо пишется по кругу, поэтому в массиве halfwaves_for_detection самая
 * старая полуволна может стоять в середине. Ищем точку разрыва по времени
 * старта и переписываем элементы, начиная с неё.
 *
 * @param used_scope Осциллограф, чей буфер полуволн разворачивается
 * @param out_halfwaves Приёмник линейной копии (не меньше PERIOD_DETECTOR_BUFFER_SIZE)
 *
 * @return Количество реально скопированных полуволн
 *
 */
static int linearize_halfwaves(Scope* used_scope, halfwave_data_ctx* out_halfwaves)
{
    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;

    int count = buffer->count;

    if (count > PERIOD_DETECTOR_BUFFER_SIZE) count = PERIOD_DETECTOR_BUFFER_SIZE;
    if (count <= 0) return 0;


    // Ищем место, где время старта пошло назад - это и есть точка разрыва кольца

    int pivot = 0;

    for (int i = 1; i < count; i++)
    {
        if (buffer->halfwaves_for_detection[i].start_time <
            buffer->halfwaves_for_detection[i - 1].start_time)
        {
            pivot = i;
            break;
        }
    }


    for (int i = 0; i < count; i++)
    {
        int source_index = (pivot + i) % count;

        out_halfwaves[i] = buffer->halfwaves_for_detection[source_index];
    }

    return count;
}


/**
 * @brief Раскладывает полуволны по классам "похожести" (элементам паттерна).
 *
 * Для каждой полуволны ищется БЛИЖАЙШИЙ уже существующий класс того же
 * направления. Если ближайший всё равно дальше HALFWAVE_MATCH_TRESHOLD -
 * заводится новый класс. Эталон принявшего класса подтягивается к элементу
 * с коэффициентом HALFWAVE_CLASS_BLEND, чтобы одиночный шумовой выброс
 * не утаскивал за собой весь класс.
 *
 * @param used_scope Осциллограф (нужен для нормировки в halfwave_distance)
 * @param halfwaves Линейная копия полуволн
 * @param count Количество полуволн
 * @param out_labels Приёмник номеров классов (нумерация с 1)
 *
 * @return Количество найденных классов
 *
 */
static int classify_halfwaves(

    Scope* used_scope,
    const halfwave_data_ctx* halfwaves,
    int count,
    int* out_labels

)
{
    halfwave_data_ctx class_reference[PERIOD_DETECTOR_BUFFER_SIZE];

    int classes_count = 0;


    for (int i = 0; i < count; i++)
    {
        int best_class = -1;
        float best_distance = HALFWAVE_MATCH_TRESHOLD;


        for (int c = 0; c < classes_count; c++)
        {
            // Сравнивать восходящую полуволну с нисходящей бессмысленно
            if (class_reference[c].halfwave_type != halfwaves[i].halfwave_type) continue;


            float current_distance = halfwave_distance(used_scope, &class_reference[c], &halfwaves[i]);

            if (current_distance < best_distance)
            {
                best_distance = current_distance;
                best_class = c;
            }
        }


        // Ни один класс не подошёл - заводим новый

        if (best_class < 0)
        {
            if (classes_count >= PERIOD_DETECTOR_BUFFER_SIZE)
            {
                // Классов физически не может быть больше, чем полуволн,
                // но защита от выхода за массив всё равно нужна
                out_labels[i] = classes_count;
                continue;
            }

            class_reference[classes_count] = halfwaves[i];

            classes_count += 1;

            out_labels[i] = classes_count;

            continue;
        }


        // Класс найден - записываем метку и подтягиваем эталон

        out_labels[i] = best_class + 1;


        halfwave_data_ctx* reference = &class_reference[best_class];

        reference->halfwave_full_time +=
            HALFWAVE_CLASS_BLEND * (halfwaves[i].halfwave_full_time - reference->halfwave_full_time);

        reference->peak_value +=
            HALFWAVE_CLASS_BLEND * (halfwaves[i].peak_value - reference->peak_value);

        reference->trough_value +=
            HALFWAVE_CLASS_BLEND * (halfwaves[i].trough_value - reference->trough_value);

        reference->halfwave_area +=
            HALFWAVE_CLASS_BLEND * (halfwaves[i].halfwave_area - reference->halfwave_area);

        reference->halfwave_smoothed_speed +=
            HALFWAVE_CLASS_BLEND * (halfwaves[i].halfwave_smoothed_speed - reference->halfwave_smoothed_speed);
    }


    return classes_count;
}


/**
 * @brief Символьная автокорреляция строки классов для длины периода L.
 *
 * Сравниваются только те пары (i, i + L), которые ЦЕЛИКОМ лежат внутри
 * реальных данных. Именно отсутствие этого условия в старой версии
 * (сравнение уходило в удвоенную копию буфера) и завышало оценку
 * для больших L.
 *
 * @param labels Массив номеров классов
 * @param count Количество элементов
 * @param L Проверяемая длина периода в полуволнах
 *
 * @return Доля совпавших пар в диапазоне [0;1]
 *
 */
static float symbolic_pattern_score(const int* labels, int count, int L)
{
    int compared_elements = count - L;

    if (compared_elements <= 0) return 0.0f;


    int equal_elements = 0;

    for (int i = 0; i < compared_elements; i++)
    {
        if (labels[i] == labels[i + L]) equal_elements += 1;
    }


    return (float)equal_elements / (float)compared_elements;
}


/**
 * @brief ВременнАя автокорреляция длительностей полуволн для длины периода L.
 *
 * Второй, независимый от классификации свидетель. Даже если раскраска
 * полуволн сбилась из-за шума, сами длительности всё равно повторяются
 * с шагом истинного периода, поэтому эта метрика вытягивает детекцию там,
 * где символьная уже врёт.
 *
 * Считается нормированная относительная невязка: сумма модулей разностей,
 * делённая на сумму средних. Результат переводится в "качество" как 1 - ошибка.
 *
 * @param halfwaves Линейная копия полуволн
 * @param count Количество полуволн
 * @param L Проверяемая длина периода в полуволнах
 *
 * @return Качество совпадения длительностей в диапазоне [0;1]
 *
 */
static float temporal_pattern_score(const halfwave_data_ctx* halfwaves, int count, int L)
{
    int compared_elements = count - L;

    if (compared_elements <= 0) return 0.0f;


    double total_error = 0.0;
    double total_scale = 0.0;


    for (int i = 0; i < compared_elements; i++)
    {
        double time_a = halfwaves[i].halfwave_full_time;
        double time_b = halfwaves[i + L].halfwave_full_time;

        total_error += fabs(time_a - time_b);
        total_scale += 0.5 * (fabs(time_a) + fabs(time_b));
    }


    if (total_scale < 1e-12) return 0.0f;


    float relative_error = (float)(total_error / total_scale);

    float score = 1.0f - relative_error;

    if (score < 0.0f) score = 0.0f;

    return score;
}


int detect_pattern(Scope* used_scope)
{
    if (TEST_MODE_5)
    {
        printf("\n\nПоиск паттерна начат!!!\n");
    }


    scope_measured_signal_data_ctx* measured_characteristics =
        &used_scope->signal_control_data.measured_signal_characteristics;


    // ===== Подготовка буффера =====

    // Разворачиваем кольцо в линейную последовательность по времени
    int count = linearize_halfwaves(used_scope, linear_halfwaves);

    // Для перебора нужно минимум PATTERN_MIN_REPEATS повторов
    // самого короткого возможного паттерна (две полуволны)
    if (count < 2 * PATTERN_MIN_REPEATS)
    {
        measured_characteristics->pattern_confidence = 0.0f;

        return 0;
    }

    // ===== Подготовка буффера =====


    // ===== Классификация полуволн =====

    int classes_count = classify_halfwaves(used_scope, linear_halfwaves, count, halfwave_labels);


    if (TEST_MODE_5 || TEST_MODE_6)
    {
        printf("\nТекущий шаблон (%d классов): ", classes_count);

        for (int i = 0; i < count; i++)
        {
            printf("%d", halfwave_labels[i]);
        }

        printf("\n\n");
    }

    // ===== Классификация полуволн =====


    // ===== Перебор кандидатов =====

    /*

        Перебираем только ЧЁТНЫЕ длины: за один период периодического сигнала
        происходит поровну переходов вверх и вниз, значит полуволн в периоде
        всегда чётное число.

        Верхняя граница - count / PATTERN_MIN_REPEATS, а не count / 2.

        Разница принципиальная. При count = 64 и потолке count / 2 кандидат
        L = 30 успевает "повториться" всего дважды и оценивается всего
        по 34 парам. На шумном сигнале такая короткая выборка легко даёт
        случайно высокую оценку - и детектор радостно объявляет период
        в пять раз длиннее настоящего. Требование трёх повторов сокращает
        потолок до 21 и убирает этот класс ложных срабатываний целиком.

    */

    int max_pattern_length = count / PATTERN_MIN_REPEATS;

    if (max_pattern_length > PERIOD_DETECTOR_BUFFER_SIZE) max_pattern_length = PERIOD_DETECTOR_BUFFER_SIZE;


    // Оценки обоих свидетелей и их свёртка для каждого кандидата
    float symbolic_scores[PERIOD_DETECTOR_BUFFER_SIZE + 1];
    float temporal_scores[PERIOD_DETECTOR_BUFFER_SIZE + 1];
    float combined_scores[PERIOD_DETECTOR_BUFFER_SIZE + 1];

    for (int L = 0; L <= PERIOD_DETECTOR_BUFFER_SIZE; L++)
    {
        symbolic_scores[L] = 0.0f;
        temporal_scores[L] = 0.0f;
        combined_scores[L] = -1.0f;
    }


    int best_pattern_len = 0;
    float best_score = -1.0f;


    for (int L = 2; L <= max_pattern_length; L += 2)
    {
        symbolic_scores[L] = symbolic_pattern_score(halfwave_labels, count, L);
        temporal_scores[L] = temporal_pattern_score(linear_halfwaves, count, L);

        combined_scores[L] =
            (PATTERN_SYMBOLIC_WEIGHT * symbolic_scores[L]) +
            (PATTERN_TEMPORAL_WEIGHT * temporal_scores[L]);


        if (TEST_MODE_5)
        {
            printf(
                "L=%2d  score=%f  (sym=%f  time=%f)\n",
                L,
                combined_scores[L],
                symbolic_scores[L],
                temporal_scores[L]
            );
        }


        if (combined_scores[L] > best_score)
        {
            best_score = combined_scores[L];
            best_pattern_len = L;
        }
    }


    if (best_pattern_len < 2)
    {
        measured_characteristics->pattern_confidence = 0.0f;

        return 0;
    }

    // ===== Перебор кандидатов =====


    // ===== Схлопывание кратностей =====

    /*

        Берём САМУЮ КОРОТКУЮ длину, качество которой не хуже лучшего
        больше чем на PATTERN_DIVISOR_TOLERANCE.

        Это обобщение схлопывания делителей. Строка 121212... даёт одинаково
        хорошую оценку для L = 2, 4, 6, 8 - и без этого шага победителем
        становится случайный из них, а показания периода прыгают кратно.
        Правило "при равном качестве выигрывает самый короткий" - стандартная
        формулировка задачи поиска фундаментального периода строки.

        Работает и для не-делителей: если L = 14 и L = 2 показывают почти
        одинаковое качество, физического смысла в четырнадцати полуволнах нет.

    */

    for (int L = 2; L < best_pattern_len; L += 2)
    {
        if (combined_scores[L] >= best_score - PATTERN_DIVISOR_TOLERANCE)
        {
            if (TEST_MODE_5)
            {
                printf("Схлопывание кратности: %d -> %d (%f вместо %f)\n",
                    best_pattern_len, L, combined_scores[L], best_score);
            }

            best_pattern_len = L;
            best_score = combined_scores[L];

            break;
        }
    }

    // ===== Схлопывание кратностей =====


    // ===== Гейт на приём =====

    // Сколько буфера покрывают ЦЕЛЫЕ повторы найденного паттерна
    float coverage = (float)((count / best_pattern_len) * best_pattern_len) / (float)count;

    // Слабейший из двух свидетелей
    float weakest_witness = fminf(symbolic_scores[best_pattern_len], temporal_scores[best_pattern_len]);


    if (TEST_MODE_5)
    {
        printf("\nЛучший период = %d\n", best_pattern_len);
        printf("Качество      = %f\n", best_score);
        printf("Слабый свид.  = %f\n", weakest_witness);
        printf("Покрытие      = %f\n", coverage);
    }


    if (best_score < PATTERN_MIN_SCORE ||
        weakest_witness < PATTERN_MIN_WITNESS ||
        coverage < PATTERN_MIN_COVERAGE)
    {
        // Не убедили. Период НЕ обновляем - на дисплее останется
        // прошлое достоверное значение, а не свежее случайное

        measured_characteristics->pattern_confidence = best_score;

        if (TEST_MODE_5)
        {
            printf("Паттерн отклонён гейтом!\n");
        }

        return 0;
    }


    measured_characteristics->pattern_confidence = best_score;
    measured_characteristics->pattern_halfwaves = best_pattern_len;

    // ===== Гейт на приём =====


    return best_pattern_len;
}


float detect_period(Scope* used_scope, int pattern_steps)
{
    if (TEST_MODE_5)
    {
        printf("Поиск периода!\n");
    }


    if (pattern_steps < 2) return 0.0f;


    // Работаем с той же линейной копией, которую только что подготовил
    // detect_pattern - разворачивать кольцо второй раз незачем
    int count = linearize_halfwaves(used_scope, linear_halfwaves);

    if (count < pattern_steps) return 0.0f;


    // ===== Сбор всех целых периодов, которые влезли в буфер =====

    float periods[PERIOD_DETECTOR_BUFFER_SIZE];

    int periods_count = 0;


    for (int start = 0; start + pattern_steps <= count; start += pattern_steps)
    {
        int end = start + pattern_steps - 1;

        double period_value =
            linear_halfwaves[end].end_time -
            linear_halfwaves[start].start_time;

        // Отрицательное или нулевое время - следствие разрыва кольца, пропускаем
        if (period_value <= 0.0) continue;

        periods[periods_count] = (float)period_value;

        periods_count += 1;
    }


    if (periods_count == 0)
    {
        fprintf(stderr, "Counter error!\n");

        return 0.0f;
    }

    // ===== Сбор всех целых периодов, которые влезли в буфер =====


    // ===== Медиана вместо среднего =====

    // Среднее по буферу ловит любой одиночный сбойный период целиком
    // (например, пропущенный zero-cross удваивает одно измерение и тянет
    // среднее вверх на 1/N). Медиана к таким выбросам нечувствительна

    qsort(periods, periods_count, sizeof(float), compare_float);

    float median_period;

    if (periods_count % 2 == 0)
    {
        median_period = 0.5f * (periods[periods_count / 2 - 1] + periods[periods_count / 2]);
    }
    else median_period = periods[periods_count / 2];


    if (TEST_MODE_5)
    {
        printf("Периодов собрано: %d\n", periods_count);
        printf("Найденный период: %f\n", median_period);
    }

    // ===== Медиана вместо среднего =====


    return median_period;
}


void detect_pattern_and_period(Scope* used_scope)
{
    if (TEST_MODE_5)
    {
        printf("Поиск паттерна!\n");
    }

    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;

    scope_measured_signal_data_ctx* measured_characteristics =
        &used_scope->signal_control_data.measured_signal_characteristics;


    // Недостаточно данных для анализа.
    //
    // Раньше здесь стояло && вместо ||, из-за чего условие выхода срабатывало
    // только при ОДНОВРЕМЕННО нечётном и слишком малом количестве полуволн.
    // При count == 0 проверка пропускала дальше, и следом объявлялся
    // VLA нулевой длины - неопределённое поведение

    if ((buffer->count % 2 != 0) || (buffer->count < PERIOD_DETECTOR_BUFFER_SIZE / 16))
    {
        if (TEST_MODE_5)
        {
            printf("Набрали в буффер: %d\n", buffer->count);
        }

        return;
    }


    // Определили паттерн
    int pattern_steps = detect_pattern(used_scope);

    if (TEST_MODE_5)
    {
        printf("Шагов паттерна: %d\n", pattern_steps);
    }


    if (pattern_steps < 2)
    {
        if (TEST_MODE_5)
        {
            printf("Паттерн не обнаружен!\n");
        }

        return;
    }


    // ===== Запись нового периода в кольцо измерений =====

    float new_period = detect_period(used_scope, pattern_steps);

    if (new_period <= 0.0f || !isfinite(new_period)) return;


    measured_periods_buffer* measured_periods = &measured_characteristics->measured_periods;

    const int PERIODS_BUFFER_SIZE = PERIOD_DETECTOR_BUFFER_SIZE / 4;


    measured_periods->periods[measured_periods->head] = new_period;

    measured_periods->head = (measured_periods->head + 1) % PERIODS_BUFFER_SIZE;

    if (measured_periods->count < PERIODS_BUFFER_SIZE) measured_periods->count += 1;

    // ===== Запись нового периода в кольцо измерений =====


    // ===== Медиана кольца измерений с отбраковкой по MAD =====

    /*

        Раньше здесь считалось среднее, а потом из него грубо выбрасывался
        максимум, если он превышал среднее втрое. Это не помогало: пара сбойных
        измерений подряд уводила среднее так, что "втрое" уже не срабатывало.

        Стандартный робастный приём - median absolute deviation:

            1) берём медиану всех измерений;
            2) считаем медиану модулей отклонений от неё (MAD);
            3) выбрасываем всё, что дальше 3 * MAD;
            4) усредняем то, что осталось.

        MAD, в отличие от дисперсии, сам по себе не портится выбросами,
        поэтому шаг 3 остаётся осмысленным даже когда половина измерений плохая.

    */

    float sorted_periods[PERIOD_DETECTOR_BUFFER_SIZE / 4];

    for (int i = 0; i < measured_periods->count; i++)
    {
        sorted_periods[i] = measured_periods->periods[i];
    }

    qsort(sorted_periods, measured_periods->count, sizeof(float), compare_float);


    float median_period = sorted_periods[measured_periods->count / 2];


    // MAD

    float deviations[PERIOD_DETECTOR_BUFFER_SIZE / 4];

    for (int i = 0; i < measured_periods->count; i++)
    {
        deviations[i] = fabsf(sorted_periods[i] - median_period);
    }

    qsort(deviations, measured_periods->count, sizeof(float), compare_float);

    float mad = deviations[measured_periods->count / 2];

    // Если разброс нулевой (все измерения совпали), допускаем 1% коридор
    float allowed_deviation = fmaxf(3.0f * mad, 0.01f * median_period);


    float accepted_sum = 0.0f;
    int accepted_count = 0;

    for (int i = 0; i < measured_periods->count; i++)
    {
        if (fabsf(sorted_periods[i] - median_period) <= allowed_deviation)
        {
            accepted_sum += sorted_periods[i];
            accepted_count += 1;
        }
    }


    float filtered_period = (accepted_count > 0) ?
        (accepted_sum / (float)accepted_count) :
        median_period;

    // ===== Медиана кольца измерений с отбраковкой по MAD =====


    // ===== Гистерезис показаний =====

    // Пока новое значение отличается от текущего меньше чем на
    // PERIOD_HYSTERESIS_PART, оставляем старое. Без этого последняя цифра
    // на дисплее мельтешит каждый кадр, хотя сигнал не менялся

    float current_period = measured_characteristics->measured_period;

    bool period_accepted = true;

    if (current_period > 0.0f)
    {
        float relative_difference = fabsf(filtered_period - current_period) / current_period;

        if (relative_difference < PERIOD_HYSTERESIS_PART) period_accepted = false;
    }


    if (period_accepted)
    {
        measured_characteristics->measured_period = filtered_period;

        // Частота считается только от заведомо ненулевого периода
        if (filtered_period > 0.0f)
        {
            float frequency = 1.0f / filtered_period;

            // До ближайшего 0.5
            measured_characteristics->measured_frequency = roundf(frequency * 2.0f) / 2.0f;
        }
    }

    // ===== Гистерезис показаний =====
}

void measured_data_update(Scope* used_scope)
{

    if (TEST_MODE_5) {

        printf("Обновление measured!\n");
        
    }


    // Обновляем недостающую дату

    /*
        typedef struct scope_measured_signal_data_ctx {

            // Текущая степень доверия к running-характеристикам (под настройку alpha и betha
            // mean_part_in_offset, median_part_in_offset)
            float current_confidence_to_running;     

            float measured_period;       // Всегда в секундах
            float measured_frequency;    // Всегда в герцах

            float measured_mean;
            float measured_median;

            float measured_max;
            float measured_min;

            float measured_dc_offset;

        } scope_measured_signal_data_ctx;

    
    */

    // По паре периодов через буффер

    /*
    
    // Одиночный сэмпл основного буффера
    typedef struct sample_t {

        float value;       // Значение сигнала
        double time;       // Время приёма сигнала
        double delta_t;    // Разница между временем приёма текущего сигнала и прошлого

    } sample_t;


    // Основной буффер
    typedef struct scope_buffer {

        sample_t samples[BUFFER_SIZE];

        int head;                                      // Текущий индекс (для записи и чтения)
        int count;                                     // Счётчик (для контроля переполнений)

    } scope_buffer_ctx;

    */


    scope_measured_signal_data_ctx* measured_parameters = &used_scope->signal_control_data.measured_signal_characteristics;

    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;


    // Пытаемся собрать дату по 4 периодам, идя назад от [head - 1] буффера

    if (measured_parameters->measured_frequency <= 0.0f) return;

    // Защита от нечисловых значений: до появления этой проверки NaN или
    // бесконечность в измеренной частоте проходили дальше и превращались
    // в мусорный размер окна
    if (!isfinite(measured_parameters->measured_frequency)) return;

    if (buffer->count < 4) return;


    int samples_per_period = (int)(SCOPE_SAMPLE_RATE / measured_parameters->measured_frequency);

    if (samples_per_period < 1) samples_per_period = 1;


    // int, а не unsigned: дальше эта величина сравнивается со знаковыми
    // счётчиками обхода, и смешивание знаковости давало предупреждения
    // компилятора и потенциально неверные сравнения
    int periods_for_check;

    if (measured_parameters->measured_frequency < 100)
    {
        periods_for_check = 1;
    }
    else if (measured_parameters->measured_frequency > 100 && measured_parameters->measured_frequency < 1000)
    {
        periods_for_check = 2;
    }
    else
    {
        periods_for_check = 4;
    }


    // ===== Data declare =====

    // Mean

    float total_samples_value = 0.0f;

    
    // Median

    // Верхняя оценка числа сэмплов для окна измерения.
    // Частота является измеренной величиной и может отличаться от фактической,
    // поэтому оставляем запас, чтобы гарантированно вместить все точки окна.

    /*

        КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ - ЗДЕСЬ ПАДАЛА ПРОГРАММА

        Было:

            unsigned int samples_for_check = samples_per_period * periods_for_check + 1000;
            float values[samples_for_check];        // VLA НА СТЕКЕ

        Размер массива напрямую зависел от ИЗМЕРЕННОЙ частоты, а измеренная
        частота - это как раз то, что детектор периода регулярно определял
        неправильно. Стоило ему выдать низкую частоту, и окно раздувалось:

            f = 20 Гц   ->  2400 * 1 + 1000  =  3 400 float   (13 КБ)   - ещё ничего
            f =  1 Гц   -> 48000 * 1 + 1000  = 49 000 float  (191 КБ)   - уже опасно
            f = 0.5 Гц  -> 96000 * 1 + 1000  = 97 000 float  (379 КБ)   - почти весь стек
            f = 0.1 Гц  ->             ...   = 481 000 float (1.8 МБ)   - гарантированный крах

        Стек потока по умолчанию в Windows - 1 МБ. VLA не проверяется ничем:
        компилятор просто вычитает размер из указателя стека, и запись
        в values[0] уходит за гард-страницу. Это ровно тот тип падения,
        который описан в задаче - "запись куда-то, чего нет", иногда
        с зависанием (если попадаем в гард-страницу и ОС начинает
        расширять стек), иногда без.

        Стало: буфер фиксированного размера в статической памяти, а окно
        зажимается сверху. Размер известен на этапе компиляции, стек не
        трогается вообще, поведение не зависит от качества измерения частоты.

        Буфер именно static, а не поле структуры Scope: это чисто рабочая
        память на время одного синхронного вызова, между вызовами в ней
        ничего не хранится, поэтому её можно разделить между всеми
        объектами осциллографа и не раздувать структуру на 64 КБ.

    */

    static float values[MEASURE_WINDOW_MAX_SAMPLES];

    int samples_for_check = samples_per_period * periods_for_check + 1000;

    if (samples_for_check > MEASURE_WINDOW_MAX_SAMPLES)
        samples_for_check = MEASURE_WINDOW_MAX_SAMPLES;

    // Дальше, чем накоплено реальных данных, идти всё равно нельзя
    if (samples_for_check > buffer->count) samples_for_check = buffer->count;

    if (samples_for_check < 16) return;


    float measured_median = 0.0f;


    // Extrema

    float measured_max = -FLT_MAX;

    float measured_min = FLT_MAX;


    // Init passthrough data

    float elapsed_time_on_this_period = measured_parameters->measured_period; 

    int passed_periods_counter = 0;
    int passed_samples_counter = 0;
    

    // Инициализация обязательна: цепочка if/else if ниже формально может
    // не покрыть все случаи с точки зрения компилятора, а чтение
    // неинициализированного индекса - это прямое обращение в произвольную
    // точку девятимегабайтного массива
    unsigned int curr_buffer_position = 0;
    unsigned int prev_buffer_position = 0;

    if (buffer->head >= 2)
    {
        curr_buffer_position = buffer->head - 1;
        prev_buffer_position = buffer->head - 2;
    }

    else if (buffer->head == 1)
    {
        curr_buffer_position = buffer->head - 1;
        prev_buffer_position = BUFFER_SIZE - 1;
    }

    else if (buffer->head == 0)
    {
        curr_buffer_position = BUFFER_SIZE - 1;
        prev_buffer_position = BUFFER_SIZE - 2;
    }


    // ===== Data declare =====

    // Pass states

    bool pass_through_needed_periods = false;
    bool end_of_data = false;
    bool samples_for_check_overthrow = false;

    // Обход
    while (!pass_through_needed_periods && !end_of_data && !samples_for_check_overthrow)
    {   

        sample_t curr_sample = buffer->samples[curr_buffer_position];
        sample_t prev_sample = buffer->samples[prev_buffer_position];


        // Чек закончились данные или нет
        if (curr_sample.time < prev_sample.time)
        {
            end_of_data = true;
            continue;
        }


        // Производим аккумуляцию данных

        // Mean summ update

        total_samples_value += curr_sample.value;


        // Extremas check

        if (curr_sample.value > measured_max) measured_max = curr_sample.value;

        if (curr_sample.value < measured_min) measured_min = curr_sample.value;


        // Median check 

        // Копируем значение для последующего поиска медианы
        values[passed_samples_counter] = curr_sample.value;


        // Обновляем счётчик обхода
        passed_samples_counter += 1;


        // Сдвигаемся с учётом кольца
        curr_buffer_position =
            (curr_buffer_position == 0) ? BUFFER_SIZE - 1 : curr_buffer_position - 1;

        prev_buffer_position =
            (prev_buffer_position == 0) ? BUFFER_SIZE - 1 : prev_buffer_position - 1;



        // Чек обхода периодов - по дельте можно определить
        // что следующий переход бессмысленен
        if (elapsed_time_on_this_period - curr_sample.delta_t < 0)
        {
            passed_periods_counter += 1;

            elapsed_time_on_this_period = measured_parameters->measured_period;

            // Значит на следующем шаге пойдет уже лишний период
            if (passed_periods_counter == periods_for_check)
            {
                pass_through_needed_periods = true;
                continue;
            }
        }
        else
        {
            // Дефолтно обновляем таймер
            elapsed_time_on_this_period = elapsed_time_on_this_period - curr_sample.delta_t;
        }


        // Следим, чтобы буффер не переполнился на следующем шаге
        if (passed_samples_counter >= samples_for_check)
        {
            samples_for_check_overthrow = true;
        }
    } 


    bool continue_data_update = false;
    {
    // Обрабатываем завершение обхода, исходя из выставленного флага (невозможно получить сразу 2 флага,
    // так как 2 из трёх дропают обход, через continue)
        if (pass_through_needed_periods)
        {   
            continue_data_update = true;
        }

        if (end_of_data)
        {
            // Смотрим, прошли ли мы хотя бы 1 период
            if (passed_periods_counter >= 1)
            {
                continue_data_update = true;
            }
        }

        if (samples_for_check_overthrow)
        {
            // Смотрим, прошли ли мы хотя бы 1 период
            if (passed_periods_counter >= 1)
            {
                continue_data_update = true;
            }
        }
    }


    if (continue_data_update)
    {
        // Записываем новые значения среднего и экстремумов

        measured_parameters->measured_mean = (float)(total_samples_value / passed_samples_counter); 

        measured_parameters->measured_dc_offset = measured_parameters->measured_mean;


        measured_parameters->measured_max = measured_max;
        measured_parameters->measured_min = measured_min;
        measured_parameters->measured_amplitude = measured_max - measured_min;

        // Производим поиск медианы через срезку первичного массива до 
        // заполненных данных, сортировку и значение в середине
        // Работаем с values[passed_samples_counter] только до values[passed_samples_counter - 1]

        // Сортируем только заполненную часть массива
        qsort(

            values,
            passed_samples_counter,
            sizeof(float),
            compare_float
        
        );

        // Вычисляем медиану
        if (passed_samples_counter % 2 == 0)
        {
            measured_median =
                (values[passed_samples_counter / 2 - 1] +
                values[passed_samples_counter / 2]) * 0.5f;
        }
        else
        {
            measured_median = values[passed_samples_counter / 2];
        }

        // Записываем результат
        measured_parameters->measured_median = measured_median;


        // Вычисляем offset


        return;
    }

    else return;
}


void renew_filter(Scope* used_scope)
{

    if (TEST_MODE_5) {

        printf("Обновление фильтра!\n");
        
    }


    // При входе в функцию на руках имеется два датасета



    /*

        typedef struct scope_measured_signal_data_ctx {

            // Текущая степень доверия к running-характеристикам (под настройку alpha и betha
            // mean_part_in_offset, median_part_in_offset)
            float current_confidence_to_running;     

            float measured_period;       // Всегда в секундах
            float measured_frequency;    // Всегда в герцах

            float measured_mean;
            float measured_median;

            float measured_max;
            float measured_min;

            float measured_dc_offset;

        } scope_measured_signal_data_ctx;

        и

        typedef struct scope_running_signal_data_ctx {

            running_mean_ctx running_mean;
            running_median_ctx running_median;

            // Offset, как 0.7 * median + 0.3 * mean;
            // С изменением долей по результатам сравнения measured с running
            float median_part_in_offset;
            float mean_part_in_offset;

            float running_dc_offset;

            // Используются в детекторе полуволн для расчёта скорости волны
            // при пропусках шагов буффера из-за шума
            float last_not_noise_value;
            float last_not_noise_time;

        } scope_running_signal_data_ctx;
    
    */

    // По их соотношениям необходимо вычислить текущие составные коэффициенты расхождений 
    // (локальные переменные в данной функции - временные доверия к каждой характеристике)
    // По всем им, как среднее значение можно найти:

    /*

        // Текущая степень доверия к running-характеристикам (под настройку alpha и betha
        // mean_part_in_offset, median_part_in_offset)
        
        float current_confidence_to_running;     

    */

    // И в случае, если он < 0.85 - произвести по конкретным расхождениям,  
    // сдвиг коэффициентов фильтра относительно текущих значений

    /*
            
        // Общий контекст фильтрации инпута
        typedef struct scope_realtime_filtering_ctx {

            float running_betha;
            
            float running_sigma_squad; - не трогать, изменяется в runtim-е

            // Коэффициент трешхолда
            float k_treshold;
            float running_treshold;    - не трогать, изменяется в runtim-e

        } scope_realtime_filtering_ctx;


        и поменять, исходя из разницы между измеренными медианой / средним и running медианой / средним:
        
            float median_part_in_offset;
            float mean_part_in_offset;
        
        так, чтобы running_dc_offset совпал с measured
    */

    scope_measured_signal_data_ctx* measured_parameters = &used_scope->signal_control_data.measured_signal_characteristics;
    
    scope_running_signal_data_ctx* running_parameters = &used_scope->signal_control_data.running_signal_characteristics;

    scope_realtime_filtering_ctx* filter_parameters = &used_scope->signal_control_data.filter_ctx;


    // ===== Current difference =====

    float mean_error =
        fabsf(measured_parameters->measured_mean -
            running_parameters->running_mean.mean);

    float median_error =
        fabsf(measured_parameters->measured_median -
            running_parameters->running_median.median);

    float dc_error =
        fabsf(measured_parameters->measured_dc_offset -
            running_parameters->running_dc_offset);

    // ===== Current difference =====


    // ===== Current scale =====

    /*

        ИСПРАВЛЕНО: чем нормировать невязку.

        Раньше нормировка шла на 3 * sigma, где sigma - разброс полного сигнала.
        Для синусоиды амплитудой 5 В это 3 * 3.5 = 10.5 В. Любая реальная
        невязка между measured и running (доли вольта) на фоне 10.5 В давала
        доверие 0.95-1.0, итоговый confidence всегда получался выше 0.85,
        срабатывал ранний return - и ВСЯ адаптация фильтра ниже была
        недостижимым кодом. Фильтр никогда не подстраивался.

        Нормируем на амплитуду: "насколько центр промахнулся в долях размаха
        сигнала" - это ровно тот вопрос, ответ на который нам и нужен.

    */

    float amplitude_scale = measured_parameters->measured_amplitude;

    if (!isfinite(amplitude_scale) || amplitude_scale < 1e-6f)
        amplitude_scale = used_scope->signal_control_data.peaks_ctx.running_amplitude;

    if (!isfinite(amplitude_scale) || amplitude_scale < 1e-6f) return;

    // ===== Current scale =====


    // ===== Normalized difference =====


    // Если ошибка достигла десятой части размаха, считаем доверие нулевым.

    const float MAX_ALLOWED_ERROR_PART = 0.10f;

    float error_scale = MAX_ALLOWED_ERROR_PART * amplitude_scale;


    float mean_confidence =
        1.0f - mean_error / error_scale;

    float median_confidence =
        1.0f - median_error / error_scale;

    float dc_confidence =
        1.0f - dc_error / error_scale;


    mean_confidence =
        fminf(1.0f, fmaxf(0.0f, mean_confidence));

    median_confidence =
        fminf(1.0f, fmaxf(0.0f, median_confidence));

    dc_confidence =
        fminf(1.0f, fmaxf(0.0f, dc_confidence));


    /*

        1.0  → полное совпадение

        0.8  → небольшое отличие

        0.5  → заметное расхождение

        0.0  → running вообще не соответствует measured
        
    */

    float confidence =
    (
        
        0.25 * mean_confidence +
        0.25 * median_confidence +
        0.5 * dc_confidence                 // Больший вес на работу системы

    );


    measured_parameters->current_confidence_to_running = confidence;

    // ===== Normalized difference =====


    // ===== Filter renew =====


    /*

        ШАГ, КОТОРЫЙ ВЫПОЛНЯЕТСЯ ВСЕГДА - подстройка скорости медианы.

        Она не является "коррекцией по невязке", это пересчёт кинематики
        оценщика центра под текущую частоту и амплитуду сигнала.

        Требование простое: за половину периода медиана не должна уехать
        больше чем на MAX_MEDIAN_DRIFT_PART от амплитуды. Иначе оценка
        центра начинает повторять форму волны, а вместе с центром
        уезжают и все zero-cross-ы.

            step * (samples_per_period / 2) <= MAX_MEDIAN_DRIFT_PART * A

        Отсюда прямо и выражаем шаг.

    */

    if (measured_parameters->measured_period > 0.0f &&
        isfinite(measured_parameters->measured_period))
    {
        float samples_per_period =
            (float)SCOPE_SAMPLE_RATE * measured_parameters->measured_period;

        if (samples_per_period >= 2.0f)
        {
            float target_step =
                (MAX_MEDIAN_DRIFT_PART * amplitude_scale) / (0.5f * samples_per_period);

            // Ограничиваем сверху стартовым значением: быстрее, чем
            // "весь размах за DC_TRACKING_TIME", медиане двигаться незачем
            float max_step = EXPECTED_SIGNAL_RANGE / (DC_TRACKING_TIME * (float)SCOPE_SAMPLE_RATE);

            if (target_step > max_step) target_step = max_step;
            if (target_step < 1e-8f) target_step = 1e-8f;

            running_parameters->running_median.step = target_step;
        }
    }


    if (confidence >= 0.85)
    {
        // Не требует сдвига
        return;
    }

    else
    {
        /*

            ПРЯМАЯ КОРРЕКЦИЯ ЦЕНТРА

            Раньше при расхождении менялись только веса blend-а между средним
            и медианой. Но если ОБЕ оценки уехали в одну сторону (а именно так
            и происходит при несимметричном сигнале), никакая перетасовка весов
            центр не вернёт: сколько ни смешивай два смещённых числа, результат
            останется смещённым.

            Поэтому раз в вызов подтягиваем сами оценки к measured-значениям
            на DC_CORRECTION_PART от невязки. Это остаётся "лёгкой подстройкой"
            из исходной идеи: за один шаг центр сдвигается на четверть ошибки,
            то есть полная сходимость занимает несколько десятков миллисекунд
            и на картинке не видна как скачок.

        */

        running_parameters->running_median.median +=
            DC_CORRECTION_PART * (measured_parameters->measured_median - running_parameters->running_median.median);

        running_parameters->running_mean.mean +=
            DC_CORRECTION_PART * (measured_parameters->measured_mean - running_parameters->running_mean.mean);


        // Running betha move

        if (dc_confidence < 0.8f)
        {
            filter_parameters->running_betha += BETHA_STEP;
        }
        else if (dc_confidence > 0.95f)
        {
            filter_parameters->running_betha -= BETHA_STEP;
        }


        filter_parameters->running_betha = fmaxf(

                MIN_RUNNING_BETHA,
                fminf(MAX_RUNNING_BETHA, filter_parameters->running_betha)

        );


        // Treshold coefficient move

        if (dc_confidence < 0.8f)
        {
            filter_parameters->k_treshold += K_TRESHOLD_STEP;
        }
        else if (dc_confidence > 0.95f)
        {
            filter_parameters->k_treshold -= K_TRESHOLD_STEP;
        }


        filter_parameters->k_treshold =fmaxf(
            
            MIN_K_TRESHOLD,
            fminf(MAX_K_TRESHOLD, filter_parameters->k_treshold)

        );

        // Mean-median parts in DC-offset move

        float advantage = fabsf(median_confidence - mean_confidence);

        if (median_confidence > mean_confidence)
        {
            running_parameters->median_part_in_offset += OFFSET_BLEND_STEP * advantage;
        }

        else if (mean_confidence > median_confidence)
        {
            running_parameters->median_part_in_offset -= OFFSET_BLEND_STEP * advantage;
        }

        running_parameters->median_part_in_offset = fmaxf(

            MIN_MEDIAN_PART,
            fminf(MAX_MEDIAN_PART, running_parameters->median_part_in_offset)

        );

        running_parameters->mean_part_in_offset = 1.0f - running_parameters->median_part_in_offset;

    }

    // ===== Filter renew =====
}


// Основные функции

void signal_fast_analysis(Scope* used_scope)
{   

    if (TEST_MODE_1) {

        printf("Зашли в анализ!\n");

    }

    if (used_scope->main_settings.current_state != ON_SS) return;


    
    if (TEST_MODE_1) {

        printf("Запустили анализ!\n");

    }


    // Получаем сигнал
    scope_buffer_update(used_scope);

    // Анализируем runtime-характеристики
    runtime_data_update(used_scope);

}


void signal_slow_analysis(Scope* used_scope)
{

    if (TEST_MODE_1) {

        printf("Зашли в  медленный анализ!\n");

    }

    if (used_scope->main_settings.current_state != ON_SS) return;


    if (TEST_MODE_1) {

        printf("Запустили медленный анализ!\n");

    }


    detect_pattern_and_period(used_scope);      // Анализ буффера полуволн для получение паттерна и расчёта периода

    measured_data_update(used_scope);           // Анализ нескольких последних периодов для получения measured-характеристик  

    renew_filter(used_scope);                   // Настройка фильтров по полученным measured-характеристикам
}



void scope_init(Scope* used_scope, SDL_Renderer* renderer)
{
    // ===== Инициализация настроек осциллографа ===== 
        
    scope_main_settings_init(used_scope);

    // ===== Инициализация настроек осциллографа ===== 


    // ===== Инициализация буфферов, фильтра, характеристик и детектора полуволн ===== 

    scope_signal_buffer_init(used_scope);

    scope_running_signal_characteristics_init(used_scope);
    scope_measured_signal_characteristics_init(used_scope);

    scope_filter_init(used_scope);

    scope_peaks_ctx_init(used_scope);
    scope_wave_pattern_detector_former_init(used_scope);
    scope_wave_pattern_detector_init(used_scope);

    // ===== Инициализация буфферов, фильтра, характеристик и детектора полуволн ===== 


    // ===== Инициализация GUI =====

    // scope_gui_init() внутри себя уже выставляет флаг перестройки и вызывает
    // scope_gui_renew(). Повторный вызов отсюда пересоздавал все текстбоксы
    // второй раз - вместе с 21 лишним открытым TTF_Font и 21 утечкой malloc
    scope_gui_init(used_scope, renderer);

    scope_screen_gui_init(used_scope);

    // ===== Инициализация GUI =====
}


void signal_check(Scope* used_scope, my_generator_ctx* controlled_signal)
{
    // Подключаем сигнал к осциллографу
    if (!used_scope) return;
    if (!controlled_signal) return;

    used_scope->signal_control_data.controlled_signal = controlled_signal;
    used_scope->signal_control_data.type_of_controlled_signal = CLEAN_CST;

}


void scope_fast_update(Scope* used_scope)
{
    if (TEST_MODE_1) {

        printf("Запуск быстрого апдейта!\n");

    }

    // Базовые элементы GUI - Обновляются всегда
    // Почему-то норм обновы только в рантайм скорости??????????????
    // При этом по практике лучше, когда они стоят так - перед обновлениями данных

    Textbox_update(&used_scope->scope_render_data.scope_signature_textbox, used_scope->scope_render_data.renderer);

    Textbox_update(&used_scope->scope_render_data.change_value_scale_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_update(&used_scope->scope_render_data.decrease_value_scale_button);

    Button_update(&used_scope->scope_render_data.increase_value_scale_button);


    Textbox_update(&used_scope->scope_render_data.change_time_scale_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_update(&used_scope->scope_render_data.decrease_time_scale_button);

    Button_update(&used_scope->scope_render_data.increase_time_scale_button);


    Textbox_update(&used_scope->scope_render_data.change_amplitude_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_update(&used_scope->scope_render_data.decrease_amplitude_button);

    Button_update(&used_scope->scope_render_data.increase_amplitude_button);


    Textbox_update(&used_scope->scope_render_data.change_frequency_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_update(&used_scope->scope_render_data.decrease_frequency_button);

    Button_update(&used_scope->scope_render_data.increase_frequency_button);


    Button_update(&used_scope->scope_render_data.signal_change_button);

    Button_update(&used_scope->scope_render_data.mode_change_button);

    Button_update(&used_scope->scope_render_data.controlled_signal_play_button);

    Button_update(&used_scope->scope_render_data.scope_on_off_button);


    // Основное обновление данных

    signal_fast_analysis(used_scope);

}


void scope_slow_update(Scope* used_scope)
{

    if (TEST_MODE_1) {

        printf("Запустили медленный апдейт!\n");

    }


    // Текущие текстбоксы информации о сигнале и апдейт буфера

    if (used_scope->main_settings.current_state == ON_SS)
    {
        // Обновляем текстбоксы для получения нового content, посчитанного при buffer_analysis(used_scope);
        Textbox_update(&used_scope->scope_render_data.signal_scale_textbox, used_scope->scope_render_data.renderer);

        Textbox_update(&used_scope->scope_render_data.time_scale_textbox, used_scope->scope_render_data.renderer);
    
        Textbox_update(&used_scope->scope_render_data.frequency_or_period_textbox, used_scope->scope_render_data.renderer);
    
        Textbox_update(&used_scope->scope_render_data.amplitude_textbox, used_scope->scope_render_data.renderer);
    }

    // Расчёт цвет дисплея (выключен или анимированное мерцание)
    scope_display_animation(used_scope);


    // Детект паттернов, рассчёт периода и анализ данных.
    signal_slow_analysis(used_scope);
}



void scope_destroy(Scope* used_scope)
{
    if (!used_scope) return;

    // Освобождаем шрифты и текстуры всех текстбоксов GUI.
    // Раньше деструктор их не трогал вообще: 21 TTF_Font оставался открытым
    // до самого TTF_Quit()
    scope_gui_destroy(used_scope);

    scope_main_settings_init(used_scope);

    scope_signal_buffer_clear(used_scope);

    scope_running_signal_characteristics_clear(used_scope);
    scope_measured_signal_characteristics_clear(used_scope);

    scope_filter_clear(used_scope);

    scope_peaks_ctx_clear(used_scope);
    scope_wave_pattern_detector_former_clear(used_scope);
    scope_wave_pattern_detector_clear(used_scope);

    scope_screen_gui_clear(used_scope);



    // ===== 1. сигнал рендера =====
    

    // ===== 2. UI элементы (если у них есть destroy) =====

    used_scope = NULL;
}

// =========================================================================================== API realization
