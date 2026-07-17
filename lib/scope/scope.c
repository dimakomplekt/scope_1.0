// scope.c


#define TEST_MODE_1 1


// =========================================================================================== IMPORT

#include "scope.h"

#include <stdlib.h>

#include "../app_timer/app_timer.h"
#include "../sin_generator/sin_generator.h"


#include "../../src/global_data.h"

#include <math.h>

#include <float.h>

#include <stdio.h>


// =========================================================================================== IMPORT


// =========================================================================================== Helper-functions predeclare


// ===== INIT and CLEAR helpers =====

// Init

void scope_main_settings_init(Scope* used_scope);


void scope_signal_buffer_init(Scope* used_scope);


void scope_running_signal_characteristics_init(Scope* used_scope);

void scope_measured_signal_characteristics_init(Scope* used_scope);

void scope_filter_init(Scope* used_scope);


void scope_peaks_ctx_init(Scope* used_scope);

void scope_wave_pattern_detector_former_init(Scope* used_scope);

void scope_wave_pattern_detector_init(Scope* used_scope);


void scope_gui_init(Scope* used_scope, SDL_Renderer* renderer);

void scope_screen_gui_init(Scope* used_scope);


// Clear

void scope_main_settings_clear(Scope* used_scope);


void scope_signal_buffer_clear(Scope* used_scope);


void scope_running_signal_characteristics_clear(Scope* used_scope);

void scope_measured_signal_characteristics_clear(Scope* used_scope);

void scope_filter_clear(Scope* used_scope);


void scope_peaks_ctx_clear(Scope* used_scope);

void scope_wave_pattern_detector_former_clear(Scope* used_scope);

void scope_wave_pattern_detector_clear(Scope* used_scope);


void scope_screen_gui_clear(Scope* used_scope);

// ===== INIT and CLEAR helpers =====


// ===== Scope signal analysis =====

// scope_fast_update() часть анализа =====

void scope_buffer_update(Scope* used_scope);            // Получение сигнала

void runtime_data_update(Scope* used_scope);            // Апдейт runtime-характеристик

void runtime_detect_peaks(Scope* used_scope);           // Обнаружение пиков

// Обнаружение полуволн (внутри обнаружения пиков)
void runtime_detect_halfwaves(Scope* used_scope, float current_value, float current_time, trend_type current_trend);

// Helper-функция для zero-cross детектора / halfwaves детектора
void halfwaves_detector_accumulation(Scope* used_scope, float current_value, float current_time);

// Helper-функция для zero-cross детектора / halfwaves детектора
void drop_zc_accumulation(Scope* used_scope, float current_value, float current_time);

// Helper-функция для заполнения буффера полуволн
void add_halfwave_in_buffer(Scope* used_scope, halfwave_data_ctx new_halfwave);


// scope_slow_update() часть анализа

// Анализ буффера полуволн для получение паттерна и расчёта периода
void detect_pattern_and_period(Scope* used_scope);


// Определение основного паттерна (в виде количества полуволн)
int detect_pattern(Scope* used_scope);


// Определение периода по количеству полуволн в паттерне
float detect_period(Scope* used_scope, int pattern_steps);


// Определение степени различия между двумя полуволнами
float halfwave_distance(halfwave_data_ctx* halfwave_1, halfwave_data_ctx* halfwave_2);


float normalized_difference(float a, float b);


// Сравнение 2 чисел
int compare_float(const void* a, const void* b);


// Анализ нескольких последних периодов для получения measured-характеристик
void measured_data_update(Scope* used_scope);           


 // Настройка фильтров по полученным measured-характеристикам
void renew_filter(Scope* used_scope);


// Основные функции

// Анализ поступающего сигнала для получения runtime-характеристик
// и передачи данных о пиках в детектор полуволн - коллится в scope_fast_update()
void signal_fast_analysis(Scope* used_scope);

// Анализ поступающего сигнала для получения measured-характеристик
// и передачи данных о пиках в детектор полуволн - коллится в scope_slow_update()
void signal_slow_analysis(Scope* used_scope);

// ===== Scope signal analysis =====


// ===== Утиллиты для форматирования

static float format_frequency(float f, const char** unit)
{
    if (f >= 1e6f) { *unit = "MHz"; return f * 1e-6f; }
    if (f >= 1e3f) { *unit = "kHz"; return f * 1e-3f; }
    if (f >= 1.0f)  { *unit = "Hz";  return f; }

    *unit = "mHz";
    return f * 1e3f;
}


static float format_period(float t, const char** unit)
{
    if (t >= 1.0f)      { *unit = "s";  return t; }
    if (t >= 1e-3f)     { *unit = "ms"; return t * 1e3f; }
    if (t >= 1e-6f)     { *unit = "us"; return t * 1e6f; }

    *unit = "ns";
    return t * 1e9f;
}


void format_time(Scope* used_scope, const char** unit)
{
    if (used_scope->main_settings.current_time_units == NANOSECONDS_TU)     { *unit = "ns"; }
    if (used_scope->main_settings.current_time_units == MICROSECONDS_TU)    { *unit = "mcs";}
    if (used_scope->main_settings.current_time_units == MILLISECONDS_TU)    { *unit = "ms"; }
    if (used_scope->main_settings.current_time_units == SECONDS_TU)         { *unit = "s";  }
}


static float format_voltage(float v, const char** unit)
{
    *unit = "V";
    return v;
}

// ===== Утиллиты для форматирования


// ===== Scope settings =====


// ===== Scope settings =====


// ===== Scope GUI =====

void scope_gui_renew(Scope* used_scope);                                            // Обновление графики осциллографа

void scope_screens_gui_renew_by_signal_data(Scope* used_scope);                     // Обновление данных для рендера сигнала 


void build_fixed_time_render(Scope* used_scope, signal_render_ctx* render_data);    // Helper для построения даты на рендера сигнала

void build_scroll_render(Scope* used_scope, signal_render_ctx* render_data);        // Helper для построения даты на рендера сигнала

void build_fixed_period_render(Scope* used_scope, signal_render_ctx* render_data);  // Helper для построения даты на рендера сигнала


void build_render(Scope* used_scope, signal_render_ctx* render_data);               // Helper для построения контекста данных для рендера сигнала


void scope_signal_info_gui_renew(Scope* used_scope);                                // Обновление текстбоксов в дисплее информации


void draw_signal(Scope* used_scope, SDL_Renderer* renderer);                        // Функция рендеринга сигнала


void scope_display_animation(Scope* used_scope);                                    // Расчёт анимации мерцания дисплеев

// ===== Scope GUI =====


// ===== Scope buttons callbacks =====

void decrease_scope_value_scale(Button* btn);
void increase_scope_value_scale(Button* btn);


void decrease_scope_time_scale(Button* btn);
void increase_scope_time_scale(Button* btn);


void decrease_amplitude(Button* btn);
void increase_amplitude(Button* btn);


void decrease_frequency(Button* btn);
void increase_frequency(Button* btn);


void change_scope_render_mode(Button* btn);

void change_controlled_signal(Button* btn);

void play_controlled_signal(Button* btn);


void on_off_command(Button* btn);

// ===== Scope buttons callbacks =====

// =========================================================================================== Helper-functions predeclare


// =========================================================================================== INIT and CLEAR helpers

void scope_main_settings_init(Scope* used_scope)
{
    // ===== Инициализация основных настроек ===== 

    used_scope->main_settings.current_state = OFF_SS;
    used_scope->main_settings.current_mode = SCOPE_MODE_FIXED_TIME_STEP_SRM;            // Базово - фикс
    
    used_scope->main_settings.acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
    used_scope->main_settings.acessable_modes[1] = SCOPE_MODE_SCROLL_TO_RIGHT_SRM;
    used_scope->main_settings.acessable_modes[2] = LIMIT_SRM;

    used_scope->main_settings.periods_to_display = 2;                                   // Базово - 2 периода для отображения (в режиме с фикс. кол-вом)
    used_scope->main_settings.time_val_in_one_unit = 1;                                 // Базово - 1 (режим с фикс. разв)
    used_scope->main_settings.signal_val_in_one_unit = 1;                               // Базово - 1 (режим с фикс. разв)
    
    used_scope->main_settings.current_signal_units  = VOLTS_SU;                         // Базово - вольты 
    used_scope->main_settings.current_time_units = MILLISECONDS_TU;                     // Базово - микросекунды (но переменная всегда в секундах)
    used_scope->main_settings.current_frequency_units = HERTZ_FU;                       // Базово - Герцы (но переменная всегда в Герцах)
    
    // ===== Инициализация основных настроек ===== 


    // ===== Сигнал ===== 

    // No signal at the start
    used_scope->signal_control_data.controlled_signal = NULL;

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

    // Рассчитываем максимальную скорость (первую производную) нормализованного сигнала
    // v_max = Amp * 2 * PI * f_max
    float v_max = 1.0f * 2.0f * (float)M_PI * (float)MAX_CONTROLLED_FREQ;

    // Находим максимальное приращение сигнала за ОДИН семпл
    float max_delta_per_sample = v_max / (float)SCOPE_SAMPLE_RATE;

    // ТАК КАК медиана использует систему с дрифтом (второй порядок):
    // drift += step * sign(error) -> drift *= 0.99 -> median += drift
    // Переменная 'step' работает как ускорение (дёргает скорость).
    // Экспериментальный коэффициент затухания дрифта 0.99 демпфирует разгон.
    // Чтобы медиана плавно трекала сигнал без дикого шума слежения (риппла),
    // шаг приращения скорости должен составлять около 0.1% - 0.5% от максимального дельты.
    running_data->running_median.step  = max_delta_per_sample * 0.001f; 
    
    // Начальное значение дрифта (скорости изменения) ставим в ноль,
    // чтобы алгоритм стартовал из стабильного состояния и набрал скорость сам.
    running_data->running_median.drift = 0.0f;


    // =========================================================
    // 3. FUSION & OFFSET MANAGEMENT
    // =========================================================
    running_data->median_part_in_offset = 0.3f;
    running_data->mean_part_in_offset = 0.7f;
    running_data->running_dc_offset = 0.0f;


    running_data->last_not_noise_value = 0.0f;
    running_data->last_not_noise_time = 0.0f;
}


void scope_measured_signal_characteristics_init(Scope* used_scope)
{
    scope_measured_signal_data_ctx* measured_data = &used_scope->signal_control_data.measured_signal_characteristics;


    measured_data->current_confidence_to_running = 1.0f;     

    measured_data->measured_period = 0.0f;
    measured_data->measured_frequency = 0.0f;

    measured_data->measured_mean = 0.0f;
    measured_data->measured_median = 0.0f;

    measured_data->measured_max = -FLT_MAX;
    measured_data->measured_min = FLT_MAX;

    measured_data->measured_dc_offset = 0.0f;
}


void scope_filter_init(Scope* used_scope)
{
    scope_realtime_filtering_ctx* filter = &used_scope->signal_control_data.filter_ctx;

    // Оценка шума (variance/дисперсия) должна меняться медленнее, чем само среднее значение,
    // чтобы избежать ложных срабатываний динамического порога на случайных пиках.
    // Мы закладываем время интеграции шума 0.005 с. При частоте дискретизации 48000

    // Количество сэмплов окна дисперсии N = 0.005 * 48000
    // Betha = 2 / (N + 1)

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
    filter->running_sigma_squad = 0.0001f; 

    // Множитель правила от 0.5 до 3 сигм (отсекает 99.7% случайных пиков шума)
    filter->k_treshold = 1.5f;
    
    // Рассчитываем стартовый порог сразу при инициализации
    filter->running_treshold = filter->k_treshold * filter->running_sigma_squad;
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

    peaks_ctx->running_amplitude = FLT_MAX;     // Текущая амплитуда

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


    for (int i = 0; i == PERIOD_DETECTOR_BUFFER_SIZE; i++)
    {
        wpd_ctx->halfwaves_for_detection[i].halfwave_type = STATIC_PT;
        wpd_ctx->halfwaves_for_detection[i].start_time = 0.0f;
        wpd_ctx->halfwaves_for_detection[i].end_time = 0.0f;
    
        wpd_ctx->halfwaves_for_detection[i].halfwave_full_time = 0.0f;
    
        wpd_ctx->halfwaves_for_detection[i].peak_value = -FLT_MAX;
        wpd_ctx->halfwaves_for_detection[i].trough_value = FLT_MAX;
        wpd_ctx->halfwaves_for_detection[i].halfwave_area = 0.0f;
        wpd_ctx->halfwaves_for_detection[i].halfwave_smoothed_speed = 0.0f;
    }
}


void scope_gui_init(Scope* used_scope, SDL_Renderer* renderer)
{
    // Инициализация рендерера
    used_scope->scope_render_data.renderer = renderer;


    // Строим осциллограф по сетке, используя 50 пикселей на единицу сетки 
    used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit = 50;

    // Colors
    used_scope->scope_render_data.main_color_1 = hex_to_sdl_color("#a7f109", 255);
    used_scope->scope_render_data.main_color_2 = hex_to_sdl_color("#040500", 255);
    used_scope->scope_render_data.main_color_3 = hex_to_sdl_color("#d3e8a6", 255);    
    used_scope->scope_render_data.main_color_4 = hex_to_sdl_color("#0d26e4", 255);
    used_scope->scope_render_data.main_color_5 = hex_to_sdl_color("#e63a14", 255);
    used_scope->scope_render_data.main_color_6 = hex_to_sdl_color("#313131", 220);


    used_scope->scope_render_data.basic_border_thickness_1 = 5; // (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit);
    used_scope->scope_render_data.basic_border_thickness_2 = 5; // (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit);

    if (used_scope->scope_render_data.basic_border_thickness_1 < 1) used_scope->scope_render_data.basic_border_thickness_1 = 1;
    if (used_scope->scope_render_data.basic_border_thickness_2 < 1) used_scope->scope_render_data.basic_border_thickness_2 = 1;

    // Базово - 1 вольт, 100 мкс на единицу сетки
    used_scope->scope_render_data.current_signal_scale = 1;
    used_scope->scope_render_data.current_time_scale = 100;

    // Базовые настройки GUI
    used_scope->scope_render_data.gui_parameters.width_units = 22;
    used_scope->scope_render_data.gui_parameters.height_units = 14;

    used_scope->scope_render_data.gui_parameters.bg_1_width_units = 22;
    used_scope->scope_render_data.gui_parameters.bg_1_height_units = 1;
    used_scope->scope_render_data.gui_parameters.bg_2_width_units = 22;
    used_scope->scope_render_data.gui_parameters.bg_2_height_units = 12;
    used_scope->scope_render_data.gui_parameters.bg_3_width_units = 22;
    used_scope->scope_render_data.gui_parameters.bg_3_height_units = 1;

    used_scope->scope_render_data.gui_parameters.display_width_units = 16;
    used_scope->scope_render_data.gui_parameters.display_height_units = 8;


    used_scope->scope_render_data.gui_parameters.buttons_signature_width_units = 1;
    used_scope->scope_render_data.gui_parameters.buttons_signature_height_units = 1;

    used_scope->scope_render_data.gui_parameters.buttons_1_width_units = 1;
    used_scope->scope_render_data.gui_parameters.buttons_1_height_units = 1;

    used_scope->scope_render_data.gui_parameters.buttons_2_width_units = 3;
    used_scope->scope_render_data.gui_parameters.buttons_2_height_units = 1;

    used_scope->scope_render_data.gui_parameters.info_panels_width_units = 3;
    used_scope->scope_render_data.gui_parameters.info_panels_height_units = 2;

    used_scope->scope_render_data.gui_parameters.margin_units = 1;

    // Basic position - at the center of the screen
    used_scope->scope_render_data.x_position = SCREEN_WIDTH / 2;
    used_scope->scope_render_data.y_position = SCREEN_HEIGHT / 2;


    // Расчётные настройки GUI
    used_scope->scope_render_data.scope_gui_need_update = true; 

    scope_gui_renew(used_scope);
}


void scope_screen_gui_init(Scope* used_scope)
{
    signal_render_ctx* render = &used_scope->scope_render_data.signal_render_data;

    render->size = RENDER_POINTS_BUFFER_SIZE;

    // ничего не выделяем
    // память уже существует внутри struct
}


// Clear

void scope_main_settings_clear(Scope* used_scope)
{
    // ===== Инициализация основных настроек ===== 

    used_scope->main_settings.current_mode = SCOPE_MODE_FIXED_TIME_STEP_SRM;            // Базово - фикс
    
    used_scope->main_settings.acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
    used_scope->main_settings.acessable_modes[1] = SCOPE_MODE_SCROLL_TO_RIGHT_SRM;
    used_scope->main_settings.acessable_modes[2] = LIMIT_SRM;

    used_scope->main_settings.periods_to_display = 2;                                   // Базово - 2 периода для отображения (в режиме с фикс. кол-вом)
    used_scope->main_settings.time_val_in_one_unit = 1;                                 // Базово - 1 (режим с фикс. разв)
    used_scope->main_settings.signal_val_in_one_unit = 1;                               // Базово - 1 (режим с фикс. разв)
    
    used_scope->main_settings.current_signal_units  = VOLTS_SU;                         // Базово - вольты 
    used_scope->main_settings.current_time_units = MILLISECONDS_TU;                     // Базово - микросекунды (но переменная всегда в секундах)
    used_scope->main_settings.current_frequency_units = HERTZ_FU;                       // Базово - Герцы (но переменная всегда в Герцах)
    
    // ===== Инициализация основных настроек ===== 


    // ===== Сигнал ===== 

    // No signal at the start
    used_scope->signal_control_data.controlled_signal = NULL;

    // ===== Сигнал ===== 
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


void scope_screen_gui_clear(Scope* used_scope)
{
    // Repeat init
    
    scope_gui_init(used_scope, used_scope->scope_render_data.renderer);
}


// =========================================================================================== INIT and CLEAR helpers


// =========================================================================================== SIGNAL ANALYSIS


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
    // генерацию значения сигнала
    double t = app_timer_get_time();

    // Выбор сигнала 
    float value = 0.0f;

    switch (ctrl->type_of_controlled_signal)
    {
        case CLEAN_CST:
            value = sin_generator_get_clean();
            break;

        case NOISED_CST:
            value = sin_generator_get_noise();
            break;

        default:
            value = 0.0f;
            break;
    }


    // ===== Заполнение буффера =====

    int head = buffer->head;

    // Предыдущее значение с зашитой от ошибки 1 шага
    int prev = head - 1;
    if (prev < 0)
        prev = BUFFER_SIZE - 1;

    buffer->samples[head].value = value;
    buffer->samples[head].time = t;

    if (buffer->count > 0)
    {
        double dt = t - buffer->samples[prev].time;

        // 1 сек условный clamp
        if (dt < 0 || dt > 1.0) dt = 0.0;

        // Первый сэмпл после прохода через кольцо, или любой другой
        buffer->samples[head].delta_t = dt;
    }
    else
    {
        // Первый сэмпл после init
        buffer->samples[head].delta_t = 0.0;
    }
    

    if (TEST_MODE_1) {

        printf("Записали в буффер значение: %lf\n", (double)buffer->samples[head].value);

    }


    // Update ring buffer
    buffer->head = (head + 1) % BUFFER_SIZE;

    if (buffer->count < BUFFER_SIZE) buffer->count++;

}


void runtime_data_update(Scope* used_scope)
{
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    if (buffer->count < 2) return;


    // =========================================================
    // 0. RAW SIGNAL
    // =========================================================

    // Сигнал уже записан, head сдвинут, соответственно

    int curr_idx = buffer->head - 1;
    if (curr_idx < 0) curr_idx += BUFFER_SIZE;      // Сдвиг при проходе кольца

    int prev_idx = buffer->head - 2;
    if (prev_idx < 0) prev_idx += BUFFER_SIZE;

    // Сэмплы сигнала
    sample_t curr = buffer->samples[curr_idx];
    sample_t prev = buffer->samples[prev_idx];

    float curr_x = curr.value;
    double curr_t = curr.time;
    float prev_x = prev.value;


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


    if (TEST_MODE_1) {

        printf("Новый mean: %lf\n", (double)running_data->running_mean.mean);
        
    }


    // 2.2 RUNNING MEDIAN (robust center estimator via Sign-LMS)
    float error = curr_x - running_data->running_median.median;

    running_data->running_median.median += 
        running_data->running_median.step * copysignf(1.0f, error);


    if (TEST_MODE_1) {

        printf("Новый median: %lf\n", (double)running_data->running_median.median);
        
    }


    // 2.3 running_dc (fusion center)

    running_data->running_dc_offset = (

        (running_data->median_part_in_offset * running_data->running_median.median) + 
        (running_data->mean_part_in_offset * running_data->running_mean.mean)
        
    );


    if (TEST_MODE_1) {

        printf("Новый offset: %lf\n", (double)running_data->running_dc_offset);
        
    }


    // =========================================================
    // 3. NOISE MODEL (Интеграция дисперсии шума)
    // =========================================================


        

    float sigma_squad = filter_data->running_sigma_squad;
    

    float diff;

    if (peaks_data->running_amplitude == FLT_MAX ||
        isnan(peaks_data->running_amplitude) ||
        isinf(peaks_data->running_amplitude))
    {
        diff = 0.01f; // стартовый шум
    }
    else
    {
        float signal_half_amplitude = peaks_data->running_amplitude * 0.5f;

        diff = fmaxf(signal_half_amplitude * 0.01f, 0.01f);
    }


    float diff_squad = diff * diff;


    // Пересчёт через текущую бетту и dc-offset

    sigma_squad += filter_data->running_betha * (diff_squad - sigma_squad);


    if (isnan(sigma_squad) || isinf(sigma_squad))
    {
        sigma_squad = diff_squad;
    }

    if (sigma_squad < 0.0f)
    {
        sigma_squad = 0.0f;
    }


    if (sigma_squad < 0.0f) sigma_squad = 0.0f;

    filter_data->running_sigma_squad = sigma_squad;

    float sigma = sqrt(sigma_squad);


    if (TEST_MODE_1) {

        printf("Новая sigma^2: %lf\n", (double)filter_data->running_sigma_squad);
        
    }

    // =========================================================
    // 4. DYNAMIC THRESHOLD (Расчет зоны неопределенности шума)
    // =========================================================
    // Используем константный множитель k_treshold (например, 3.0f)
    filter_data->running_treshold = filter_data->k_treshold * sigma;


    if (TEST_MODE_1) {

        printf("Новый treshold: %lf\n", (double)filter_data->running_treshold);
        
    }
    
}


void runtime_detect_peaks(Scope* used_scope)
{

    if (TEST_MODE_1) {

        printf("Заход в детектор пиков!\n");
        
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

    int curr_idx = buffer->head - 1;
    if (curr_idx < 0) curr_idx += BUFFER_SIZE;      // Сдвиг при проходе кольца

    int prev_idx = buffer->head - 2;
    if (prev_idx < 0) prev_idx += BUFFER_SIZE;

    // Сэмплы сигнала
    sample_t curr = buffer->samples[curr_idx];
    sample_t prev = buffer->samples[prev_idx];

    float curr_x = curr.value;
    double curr_t = curr.time;

    float prev_x = prev.value;
    double prev_t = prev.time;


    // =========================================================
    // 0. TREND ANALYSIS
    // =========================================================

    trend_type curr_trend;
    trend_type prev_trend;

    // Last trend (Static at init point)

    prev_trend = peaks_data->prev_trend;

    // Current trend

    float sigma = sqrt(filter->running_sigma_squad);

    float dx = curr_x - prev_x;
    float trend_eps = 0.05f * sigma;

    if ((fabsf(dx) < trend_eps)) curr_trend = STATIC_PT;
    else if (dx > 0.0f) curr_trend = RISING_PT;
    else curr_trend = FALLING_PT;

    if (TEST_MODE_1) {

        printf("Новый dx: %lf\n", (double)dx);
        
    }

    // =========================================================
    // 1. PEAKS FIXATION
    // =========================================================

    bool peak_trend_status = false;
    bool trough_trend_status = false;
    bool trend_continuation_status = false;

    // Яма
    if (prev_trend == FALLING_PT && curr_trend == RISING_PT) trough_trend_status = true;

    // Пик
    else if (prev_trend == RISING_PT && curr_trend == FALLING_PT) peak_trend_status = true;

    // Статичный тренд (2 подъема подряд, 2 опуска подряд, 2 равенства подряд)
    if (prev_trend == curr_trend) trend_continuation_status = true;


    // Проверка доверия к событию:

    // Если одинаково подряд => confidence растёт
    // Если туда-сюда => он колеблется около нуля
    // Если хаос / шум => он разрушается

    if (trend_continuation_status) peaks_data->trend_confidence += 1.0f;
    else peaks_data->trend_confidence -= 1.0f;


    // Защита от завалов уверенности
    if (peaks_data->trend_confidence >= 100.0f) peaks_data->trend_confidence = 100.0f;
    else if (peaks_data->trend_confidence <= 0.0f) peaks_data->trend_confidence = 0.0f;


    if (TEST_MODE_1) {

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

    // Новое значение - новый пик
    if (peak_trend_status)
    {
        // Прошлый кандидат становится пиком
        peaks_data->last_peak = peaks_data->peak_candidate;

        // Прошлый пик становится кандидатом
        peaks_data->peak_candidate = prev_x; 

        // Запоминаем устойчивость тренда
        // в момент фиксации экстремума.
        peaks_data->last_event_confidence = peaks_data->trend_confidence;

        // ===== Чек максимума =====

        float current_max = peaks_data->last_peak > peaks_data->peak_candidate ? peaks_data->last_peak : peaks_data->peak_candidate;

        // Обновляем running_max только если
        // новый максимум получен при более устойчивом тренде.
        bool curr_max_confidence_status = (peaks_data->max_confidence < peaks_data->trend_confidence); 

        if ((current_max > peaks_data->max_candidate) && curr_max_confidence_status)
        {   
            // Прошлый кандидат становится максимумом
            peaks_data->running_max = peaks_data->max_candidate;

            // Текущий пик становится кандидатом
            peaks_data->max_candidate = current_max;

            peaks_data->max_confidence = peaks_data->trend_confidence;


            if (TEST_MODE_1) {

                printf("Новый MAX: %lf\n", (double)peaks_data->running_max);
                
            }
        }
    } 

    // Новое значение - новая яма
    else if (trough_trend_status)
    {
        peaks_data->last_trough = peaks_data->trough_candidate;
        peaks_data->trough_candidate = prev_x; 

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
        bool curr_min_confidence_status = (peaks_data->min_confidence < peaks_data->trend_confidence); 

        if ((current_min < peaks_data->min_candidate) && curr_min_confidence_status)
        {   
            peaks_data->running_min = peaks_data->min_candidate;
            peaks_data->min_candidate = current_min;

            peaks_data->min_confidence = peaks_data->trend_confidence;


            if (TEST_MODE_1) {

                printf("Новый MIN: %lf\n", (double)peaks_data->running_min);
                
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


    // Статус zero-cross перерехода сигнала
    bool not_noise = ((deviation > 0.5 * sigma));

    if (TEST_MODE_1) {

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

    if (TEST_MODE_1) {

        printf("Решаем детектировать ли пик!\n");

        printf("buffer count = %d\n", buffer->count);
        printf("warmup = %d\n", FILTER_WARMUP_SAMPLES);
        printf("not_noise = %d\n", not_noise);
        
    }


    if (not_noise && buffer->count > FILTER_WARMUP_SAMPLES)
    {

        if (TEST_MODE_1) {

            printf("ДЕТЕКТИМ ПИК И ОБНОВЛЯЕМ ПОЛУВОЛНЫ!\n");
        
        }


        // Helper-функция по детекции zero-cross и заполнению буффера
        runtime_detect_halfwaves(used_scope, curr_x, curr_t, curr_trend);


        if (TEST_MODE_1) {

            printf("Новое значение в ZC анализ: %lf\n", (double)curr_x);
            
        }
    }

    peaks_data->prev_trend = curr_trend;
}


void runtime_detect_halfwaves(Scope* used_scope, float current_value, float current_time, trend_type current_trend)
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
    
    float sigma = sqrt(filter->running_sigma_squad);
    float noise_value = 0.5 * sigma;


    float zc_p = curr_dc_offset + curr_treshold;
    float zc_m = curr_dc_offset - curr_treshold;

    float noised_zc_mm = zc_m - noise_value;
    float noised_zc_mp = zc_m + noise_value;

    float noised_zc_pm = zc_p - noise_value;
    float noised_zc_pp = zc_p + noise_value;


    if (TEST_MODE_1)
    {
        printf(

            "\n\nDATA x=%8.4f  dc=%8.4f  thr=%8.4f  "
            "zc-=%8.4f  zc+=%8.4f  "
            "nz_mm=%8.4f  nz_mp=%8.4f  "
            "nz_pm=%8.4f  nz_pp=%8.4f\n\n\n",
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
                // Выставляем блок при нормальном переходе с учетом шума
                if (!(current_value >= noised_zc_mp))
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
                // Выставляем блок при нормальном переходе с учетом шума
                if (!(current_value >= noised_zc_pp))
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
                // Выставляем блок при нормальном переходе с учетом шума
                if (!(current_value >= noised_zc_pp))
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
                // Выставляем блок при нормальном переходе с учетом шума
                if (current_value <= noised_zc_pm)
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
                // Выставляем блок при нормальном переходе с учетом шума
                if (current_value <= noised_zc_pm)
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



void halfwaves_detector_accumulation(Scope* used_scope, float current_value, float current_time)
{

    if (TEST_MODE_1) {

        printf("Аккумуляция детектор! в ZC анализ: %lf\n", (double)current_value);
        
    }


    // Accumulate by curr and prev
    wave_pattern_detector_former_ctx* wpdf_ctx = &used_scope->signal_control_data.wave_pattern_detector_former_data;

    wave_pattern_detector_ctx* wpd_ctx = &used_scope->signal_control_data.wave_pattern_detector_data;



    float curr_x = current_value;
    float prev_x = wpdf_ctx->prev_clean_signal_value;

    float curr_time = current_time;
    float prev_time = wpdf_ctx->prev_clean_signal_time;


    // ===== Accumulation logic =====

    // Area

    wpdf_ctx->curr_halfwave.halfwave_area += curr_x;


    // Velocity

    float ds = fabsf(curr_x - prev_x);
    float dt = fabsf(curr_time - prev_time);

    float velocity; 

    
    if (dt > 0.0f)
    {
        velocity = ds / dt;
    }
    else
    {
        velocity = 0.0f;
    }

    // Собираю просто среднее значение без направления
    // TODO: узнать, насколько требуется знак (мне кажется, что не нужен)
    wpdf_ctx->curr_halfwave.halfwave_smoothed_speed = (wpdf_ctx->curr_halfwave.halfwave_smoothed_speed + fabsf(velocity)) / 2;

    
    // Peaks
    
    wpdf_ctx->curr_halfwave.peak_value = fmax(wpdf_ctx->curr_halfwave.peak_value, curr_x);

    wpdf_ctx->curr_halfwave.trough_value = fmin(wpdf_ctx->curr_halfwave.trough_value, curr_x);


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



void drop_zc_accumulation(Scope* used_scope, float current_value, float current_time)
{

    if (TEST_MODE_1) {

        printf("Дроп детектора!: %lf\n", (double)current_value);
        
    }


    wave_pattern_detector_former_ctx* wpdf_ctx = &used_scope->signal_control_data.wave_pattern_detector_former_data;
    
    halfwave_data_ctx* curr_halfwave = &wpdf_ctx->curr_halfwave;


    // ===== Чек статуса для выбора логики дропа значений =====

    zero_cross_fill_status curr_zc_status;

    if (!wpdf_ctx->halfwave_zero_crosses[0].filled) 
        curr_zc_status = TWO_ZC_EMPTY;

    else if (wpdf_ctx->halfwave_zero_crosses[0].filled && !wpdf_ctx->halfwave_zero_crosses[1].filled ) 
        curr_zc_status = FIRST_ZC_FILLED;

    // При текущей архитектуре в данной зоне нет нужды производить чек на SECOND_HALFWAVE_FILLED status

    // ===== Чек статуса для выбора логики дропа значений =====


    // ===== Заполнения zero-cross и дроп аккумуляции =====

    float curr_x = current_value;
    float prev_x = wpdf_ctx->prev_clean_signal_value;

    float curr_time = current_time;
    float prev_time = wpdf_ctx->prev_clean_signal_time;


    trend_type curr_trend = curr_halfwave->halfwave_type;



    if (curr_zc_status == TWO_ZC_EMPTY)
    {
        // Заполняем данные о первом zero-cross после инициализации

        // Чекаем предыдущие чистые значения и ищем среднее арифметическое с переданными current


        wpdf_ctx->halfwave_zero_crosses[0].zero_cross_type = curr_trend;
        wpdf_ctx->halfwave_zero_crosses[0].time = (curr_time + prev_time) * 0.5f;
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

    }


    if (curr_zc_status == FIRST_ZC_FILLED)
    {
        // Чекаем предыдущие чистые значения и ищем среднее арифметическое с переданными current

        wpdf_ctx->halfwave_zero_crosses[1].zero_cross_type = curr_trend;
        wpdf_ctx->halfwave_zero_crosses[1].time = (curr_time + prev_time) * 0.5f;
        wpdf_ctx->halfwave_zero_crosses[1].filled = true;


        curr_halfwave->start_time = wpdf_ctx->halfwave_zero_crosses[0].time;
        curr_halfwave->end_time = wpdf_ctx->halfwave_zero_crosses[1].time;

        // Разность с защитой от переполнения для нахождения времени полуволны
        if (!isnan(curr_halfwave->end_time) && !isnan(curr_halfwave->start_time)) 
        {
    
            // 2. Безопасное вычисление разности
            float diff = curr_halfwave->end_time - curr_halfwave->start_time;
            
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

        if (TEST_MODE_1)
        {
            printf(

        "\nSEND:\n"
                "peak=%f\n"
                "trough=%f\n"
                "time=%f\n"
                "area=%f\n"
                "speed=%f\n",
                curr_halfwave->peak_value,
                curr_halfwave->trough_value,
                curr_halfwave->halfwave_full_time,
                curr_halfwave->halfwave_area,
                curr_halfwave->halfwave_smoothed_speed

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

    if (TEST_MODE_1) {

        printf("Полуволна добавляется в буффер!\n");
        
    }


    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;

    // ===== Заполнение буффера =====

    int head = buffer->head;


    buffer->halfwaves_for_detection[head] = new_halfwave;


    // update ring buffer c защитой от кольцевых переходов
    buffer->head = (head + 1) % PERIOD_DETECTOR_BUFFER_SIZE;

    if (buffer->count < PERIOD_DETECTOR_BUFFER_SIZE) buffer->count++;
}


void detect_pattern_and_period(Scope* used_scope)
{
    if (TEST_MODE_1) {

        printf("Поиск паттерна!\n");
        
    }

    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;

    // Недостаточно данных или нечетное количество полуволн
    if (buffer->count % 2 != 0 || (buffer->count < PERIOD_DETECTOR_BUFFER_SIZE / 8)) 
    {
        if (TEST_MODE_1) {

            printf("Набрали в буффер: %u\n", buffer->count);
        
        }

        return;
    }

    // Определили паттерн
    int pattern_steps = detect_pattern(used_scope);

    if (TEST_MODE_1) {

        printf("Шагов паттерна: %u\n", pattern_steps);
        
    }

    if (pattern_steps < 2)
    {

        if (TEST_MODE_1) {

            printf("Паттерн не обнаружен!\n");
        
        }


        return;
    }
    else
    {
        // Нашли период
        used_scope->signal_control_data.measured_signal_characteristics.measured_period = detect_period(used_scope, pattern_steps);

        used_scope->signal_control_data.measured_signal_characteristics.measured_frequency = 1.0f / used_scope->signal_control_data.measured_signal_characteristics.measured_period;
    }
}


int detect_pattern(Scope* used_scope)
{

    if (TEST_MODE_1) {

        printf("Поиск паттерна начат!!!\n");
        
    }

    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;

    scope_measured_signal_data_ctx* measured_characteristics = &used_scope->signal_control_data.measured_signal_characteristics;


    // ===== Подготовка буффера =====

    // Обрезка буффера меньшего размера (учитываем только заполненные полуволны)
    // Заранее понятно, что буффер имеет четное количество волн

    // можно написать более быструю функцию без копий, но пока такой достаточно

    // Используем трюк с двойным буффером для увеличения шанса обнаружения паттерна
    halfwave_data_ctx curr_halfwaves_for_detection[buffer->count * 2];

    // Делаем копию заполненной даты буффера в новый двойной буффер
    for (unsigned int i = 0; i < buffer->count; i++)
    {
        curr_halfwaves_for_detection[i] = buffer->halfwaves_for_detection[i];
        curr_halfwaves_for_detection[i + buffer->count] = buffer->halfwaves_for_detection[i];
    }

    // ===== Подготовка буффера =====


    // ===== Подготовка шаблона =====

    // Работаем с curr_halfwaves_for_detection
    // Инициализируем массив int[buffer->count * 2]
    // Проходим функцией сравнения полуволн в 1 сторону, чекая пуста 
    // ли позиция полуволны в массиве символом и выдаём всем похожим 
    // полуволнам одинаковые цифры, далее инкрементим перед следующей

    int pattern_elements[buffer->count * 2];

    for (unsigned int i = 0; i < buffer->count * 2; i++)
    {
        pattern_elements[i] = 0;
    }


    unsigned int current_pattern_element_number = 1;



    if (TEST_MODE_1)
    {

        printf("\nWhole buffer: \n\n");

        for (int i = 0; i < buffer->count; i++)
        {
            printf(
                "%2d  peak=%f  trough=%f  time=%f  area=%f  speed=%f\n",
                i,
                buffer->halfwaves_for_detection[i].peak_value,
                buffer->halfwaves_for_detection[i].trough_value,
                buffer->halfwaves_for_detection[i].halfwave_full_time,
                buffer->halfwaves_for_detection[i].halfwave_area,
                buffer->halfwaves_for_detection[i].halfwave_smoothed_speed
            );
        }

        
        printf("\nWhole buffer! \n\n");
    }


    // Односторонний проход циклом for c вложенным for
    // классифицировал только первые buffer->count полуволн
    for (unsigned int i = 0; i < buffer->count; i++)
    {
        // Проверка выставлено ли уже соответствие полуволны какому-либо паттерн-элементу
        if (pattern_elements[i] != 0) continue;

        // Иначе - мы в новой полуволне
        pattern_elements[i] = current_pattern_element_number;


        if (TEST_MODE_1)
        {

            printf("\nТекущая полуволна: ");

            printf(

                "T=%f  A=%f  S=%f\n",
                curr_halfwaves_for_detection[i].halfwave_full_time,
                curr_halfwaves_for_detection[i].halfwave_area,
                curr_halfwaves_for_detection[i].halfwave_smoothed_speed

            );
        }


        // Скачем через 1 (так как волны вверх - вниз - вверх - ..., и сравнивать "вверх" с "вниз" - нехорошо)
        for (unsigned int j = i + 2; j < buffer->count * 2; j += 2)
        {
            if (pattern_elements[j] != 0) continue;

            // Ищем разницу между полуволнами
            float curr_waves_distance = halfwave_distance(&curr_halfwaves_for_detection[i], &curr_halfwaves_for_detection[j]);


            if (TEST_MODE_1) {

                printf("Найденная дистанция полуволн: %f\n", curr_waves_distance);
                
            }

            // Если она несущественна
            if (curr_waves_distance < 0.25)
            {   
                // Записываем полуволну, как соответствующую текущему элементу паттерна
                pattern_elements[j] = pattern_elements[i];
            }
        }


        // Прошли проверку по i - инкрементировали current_pattern_element_number

        current_pattern_element_number += 1;
    }

    // ===== Подготовка шаблона =====

    if (TEST_MODE_1) {

        printf("\nТекущий шаблон: ");

        for (unsigned int i = 0; i < buffer->count * 2; i++)
        {
            printf("%u", pattern_elements[i]);
        }

        printf("\n\n");
    }


    // ===== Анализ шаблона =====

    // =========================================================
    // Pattern detection
    //
    // Имеем последовательность элементов:
    //
    //      1 2 1 2 3 2 1 2 1 2 3 2
    //
    // которая была получена после сравнения полуволн.
    //
    // Требуется найти самый длинный повторяющийся шаблон.
    //
    // Для этого:
    //
    //      1. Перебираем возможную длину шаблона L
    //         от максимально возможной до минимальной.
    //
    //      2. Для каждой L проверяем все возможные начала
    //         шаблона (offset).
    //
    //      3. Сравниваем:
    //
    //              [offset ........ offset+L)
    //
    //                      и
    //
    //              [offset+L .... offset+2L)
    //
    //         Если они полностью совпадают,
    //         значит найден повторяющийся шаблон.
    //
    // Буфер заранее удвоен:
    //
    //      ABCDE -> ABCDEABCDE
    //
    // поэтому никакой обработки кольцевого перехода
    // здесь уже не требуется.
    //
    // Возвращается максимально длинный найденный шаблон.
    //
    // =========================================================

    int best_pattern_len = 0;

    int pattern_size = buffer->count;


    if (TEST_MODE_1) {

        printf("Максимальная длина паттерна: %f\n: ", buffer->count);
        
    }


    // Максимальная возможная длина периода.
    //
    // Период обязан повториться минимум два раза,
    // поэтому длиннее половины буфера быть не может.

    for (int L = pattern_size / 2; L >= 2; L--)
    {
        // Проверяем все возможные положения начала шаблона.
        //
        // Благодаря двойному буферу можем спокойно
        // двигаться до конца первого буфера.

        for (int offset = 0; offset < pattern_size; offset++)
        {
            bool pattern_equal = true;

            // Сравнение двух подряд идущих блоков длиной L.
            //
            //      offset
            //         │
            //         ▼
            //
            // 1 2 1 2 3 1 2 1 2 3
            // └───────┘└───────┘
            //     block A   block B

            for (int i = 0; i < L; i++)
            {
                if (pattern_elements[offset + i] !=
                    pattern_elements[offset + L + i])
                {
                    pattern_equal = false;
                    break;
                }
            }

            // Если найдено полное совпадение,
            // значит найден период длиной L.
            //
            // Так как перебор идёт сверху вниз,
            // это автоматически период (максимальный паттерн с повтором).

            if (pattern_equal)
            {
                best_pattern_len = L;

                // Допустимо
                goto pattern_found;
            }
        }
    }

    pattern_found:

    if (TEST_MODE_1) {

        printf("Найденная длина паттерна: %f\n: ", best_pattern_len);
        
    }


    return best_pattern_len;


    // ===== Анализ шаблона =====

}


float detect_period(Scope* used_scope, int pattern_steps)
{
    if (TEST_MODE_1) {

        printf("Поиск периода!\n");
        
    }

    wave_pattern_detector_ctx* buffer = &used_scope->signal_control_data.wave_pattern_detector_data;


    // Вводим первичные индексы

    int start_halfwave_idx = 0;
    int end_halfwave_idx = pattern_steps - 1;


    if (TEST_MODE_1) {

        printf("count=%d\n", buffer->count);
        printf("pattern=%d\n", pattern_steps);
        printf("start=%d\n", start_halfwave_idx);
        printf("end=%d\n", end_halfwave_idx);
        
    }


    if (buffer->count < pattern_steps) return 0.0f;

    // Инициируем счётчик и сумму

    int counter = 0;

    float summ_period = 0.0f;


    // Обходим буффер, сравниваясь с count
    // Проходим стартовым индексом все позиции в буффере
    while (start_halfwave_idx <= buffer->count - 1)
    {

        // Буффер может закольцеваться!

        // Обычная сумма
        if (buffer->halfwaves_for_detection[start_halfwave_idx].start_time < buffer->halfwaves_for_detection[end_halfwave_idx].end_time)
        {
            // Дополняем сумму периодов
            summ_period += 
 
                buffer->halfwaves_for_detection[end_halfwave_idx].end_time -
                buffer->halfwaves_for_detection[start_halfwave_idx].start_time;

                if (TEST_MODE_1) {

                    printf("Новая сумма периодов: .%f\n", summ_period);
                    
                }

            // Увеличиваем счётчик
            counter += 1;
        }
        else
        {
            // Cкип в случае перевёрнутого буффера - сумма будет вычислена, когда (end_halfwave_idx + pattern_steps) % buffer->count;
            // начнёт переводить конечный индекс на новое значение   
        }


        // Сдвиг к след. вычислению

        // Стоппер сдвига в случае прихода к краю
        if (start_halfwave_idx + pattern_steps >= buffer->count) break;


        // Следующий после прошлого end
        start_halfwave_idx = (end_halfwave_idx + 1) % buffer->count;

        // Сдвиг к следующему с учетом кольца
        end_halfwave_idx = (end_halfwave_idx + pattern_steps) % buffer->count;

    }

    if (counter == 0)
    {
        fprintf(stderr, "Counter error!\n");
        return 0.0f;
    }


    if (TEST_MODE_1) {

        printf("Найденная сумма периодов: .%f\n", summ_period);
        printf("Найденный период: .%f\n", summ_period / (float)counter);
    }


    // Возвращаем среднее значение
    return (summ_period / (float)counter); 
}


float halfwave_distance(halfwave_data_ctx* halfwave_1, halfwave_data_ctx* halfwave_2)
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

    float area_dist =

        fabsf(halfwave_1->halfwave_area -
              halfwave_2->halfwave_area) /

        fmaxf(
            fmaxf(fabsf(halfwave_1->halfwave_area),
                  fabsf(halfwave_2->halfwave_area)),
            1e-6f
        );


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
    
    float total = //0.2f * amp_dist
                  0.4f * time_dist
                + 0.3f * area_dist
                + 0.3f * speed_dist;

    // =========================================================
    // 6. FINAL DISTANCE
    // =========================================================
    // Формируем единую меру различия.
    // Чем ближе результат к нулю, тем более похожи полуволны.
    // Вес амплитуды снижен, так как для распознавания паттернов
    // важнее форма, чем абсолютный уровень сигнала.

    if (TEST_MODE_1)
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


void measured_data_update(Scope* used_scope)
{

    if (TEST_MODE_1) {

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

    int samples_per_period = (int)(SCOPE_SAMPLE_RATE / measured_parameters->measured_frequency);


    unsigned int periods_for_check;

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

    unsigned int samples_for_check = samples_per_period * periods_for_check + 1000;


    float values[samples_for_check];

    float measured_median = 0.0f;


    // Extrema

    float measured_max = -FLT_MAX;

    float measured_min = FLT_MAX;


    // Init passthrough data

    float elapsed_time_on_this_period = measured_parameters->measured_period; 

    int passed_periods_counter = 0;
    int passed_samples_counter = 0;
    

    unsigned int curr_buffer_position;
    unsigned int prev_buffer_position;
    
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

    if (TEST_MODE_1) {

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


    // ===== Current sigma =====

    float sigma = sqrtf(filter_parameters->running_sigma_squad);

    if (sigma < 1e-6f) sigma = 1e-6f;

    // ===== Current sigma =====


    // ===== Normalized difference =====


    // Если ошибка достигла трёх сигм, считаем доверие нулевым.

    float mean_confidence =
        1.0f - mean_error / (MAX_ALLOWED_DEVIATION_IN_SIGMAS * sigma);

    float median_confidence =
        1.0f - median_error / (MAX_ALLOWED_DEVIATION_IN_SIGMAS * sigma);

    float dc_confidence =
        1.0f - dc_error / (MAX_ALLOWED_DEVIATION_IN_SIGMAS * sigma);

        
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

    if (confidence >= 0.85)
    {
        // Не требует сдвига
        return;
    }

    else
    {
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
            filter_parameters->k_treshold += K_THRESHOLD_STEP;
        }
        else if (dc_confidence > 0.95f)
        {
            filter_parameters->k_treshold -= K_THRESHOLD_STEP;
        }


        filter_parameters->k_treshold =fmaxf(
            
            MIN_K_THRESHOLD,
            fminf(MAX_K_THRESHOLD, filter_parameters->k_treshold)

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

    // Анализируем пики
    // и детектируем полуволны
    runtime_detect_peaks(used_scope);
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

// =========================================================================================== SIGNAL ANALYSIS


// =========================================================================================== Scope GUI

void scope_gui_renew(Scope* used_scope)
{
    // Установить sizes, positions, colors, contents и прочее для всех объектов GUI в зависимости от текущих настроек 
    if (!used_scope->scope_render_data.scope_gui_need_update) return;


    // Main figures data set by scale, unit sizes and positions


    // BG 2 - основной задник, на котором располагается дисплей и кнопки - сидит по центру
  
    used_scope->scope_render_data.gui_parameters.background_x_2 = used_scope->scope_render_data.x_position;
    used_scope->scope_render_data.gui_parameters.background_y_2 = used_scope->scope_render_data.y_position;

    used_scope->scope_render_data.gui_parameters.background_w_2 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.bg_2_width_units);

    used_scope->scope_render_data.gui_parameters.background_h_2 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.bg_2_height_units);


    used_scope->scope_render_data.gui_parameters.background_fill_color_2 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.background_border_color_2 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.background_border_thickness_2 = used_scope->scope_render_data.basic_border_thickness_1;


    // BG 1 - верхний задник, на котором располагается название дисплея, сидит сверху с отступом в 0.5 единиц (центр-центр) от верхнего края экрана

    used_scope->scope_render_data.gui_parameters.background_x_1 = used_scope->scope_render_data.x_position;

    used_scope->scope_render_data.gui_parameters.background_y_1 = (used_scope->scope_render_data.y_position - 
        (used_scope->scope_render_data.gui_parameters.background_h_2 + 
        used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit) * 0.5) + used_scope->scope_render_data.gui_parameters.background_border_thickness_2;


    used_scope->scope_render_data.gui_parameters.background_w_1 = used_scope->scope_render_data.gui_parameters.background_w_2;

    used_scope->scope_render_data.gui_parameters.background_h_1 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.bg_1_height_units);


    used_scope->scope_render_data.gui_parameters.background_fill_color_1 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.background_border_color_1 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.background_border_thickness_1 = used_scope->scope_render_data.basic_border_thickness_1;


    // Текстбокс с названием дисплея
    used_scope->scope_render_data.scope_signature_textbox = *Textbox_init(used_scope->scope_render_data.main_color_2, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));
    // used_scope->scope_render_data.scope_signature_textbox.x = used_scope->scope_render_data.gui_parameters.background_x_1; - позже по линии
    used_scope->scope_render_data.scope_signature_textbox.y = used_scope->scope_render_data.gui_parameters.background_y_1;

    // Костыль чтобы не делать anchor points
    Textbox_set_content(&used_scope->scope_render_data.scope_signature_textbox, "S C O P E                                                    ");


    // BG 3 - нижний задник

    used_scope->scope_render_data.gui_parameters.background_x_3 = used_scope->scope_render_data.x_position;

    used_scope->scope_render_data.gui_parameters.background_y_3 = (used_scope->scope_render_data.y_position + 
        (used_scope->scope_render_data.gui_parameters.background_h_2 + 
        used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit) * 0.5) - used_scope->scope_render_data.gui_parameters.background_border_thickness_2;


    used_scope->scope_render_data.gui_parameters.background_w_3 = used_scope->scope_render_data.gui_parameters.background_w_2;

    used_scope->scope_render_data.gui_parameters.background_h_3 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.bg_3_height_units);


    used_scope->scope_render_data.gui_parameters.background_fill_color_3 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.background_border_color_3 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.background_border_thickness_3 = used_scope->scope_render_data.basic_border_thickness_1;


    // Дисплей - по размерам и отступу от края 

    used_scope->scope_render_data.gui_parameters.display_w = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.display_width_units);

    used_scope->scope_render_data.gui_parameters.display_h = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.display_height_units);

    int background_2_left_border_x = used_scope->scope_render_data.gui_parameters.background_x_2 - (used_scope->scope_render_data.gui_parameters.background_w_2 * 0.5);
    int bacground_2_top_border_y = used_scope->scope_render_data.gui_parameters.background_y_2 - (used_scope->scope_render_data.gui_parameters.background_h_2 * 0.5);
    int margin_in_pixels = used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * used_scope->scope_render_data.gui_parameters.margin_units;

    used_scope->scope_render_data.gui_parameters.display_x = background_2_left_border_x + margin_in_pixels + used_scope->scope_render_data.gui_parameters.display_w * 0.5;
    used_scope->scope_render_data.gui_parameters.display_y = bacground_2_top_border_y + margin_in_pixels + used_scope->scope_render_data.gui_parameters.display_h * 0.5;


    used_scope->scope_render_data.gui_parameters.display_fill_color = used_scope->scope_render_data.main_color_3;

    used_scope->scope_render_data.gui_parameters.display_border_color = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.display_border_thickness = used_scope->scope_render_data.basic_border_thickness_2;

    used_scope->scope_render_data.gui_parameters.lines_thickness = used_scope->scope_render_data.gui_parameters.display_border_thickness * 0.5;
    
    // Сетка дисплея

    // Считаем координаты границ дисплея для удобства в дальнейшем

    /*  

        Форма по точкам такая:


        [2][1]---[2][2]
         |           |
         |           |
         |           |
        [1][1]---[2][1]
    
    */
    
    int scope_low_border_x_1 = used_scope->scope_render_data.gui_parameters.display_x - 0.5 * used_scope->scope_render_data.gui_parameters.display_w;
    int scope_low_border_y_1 = used_scope->scope_render_data.gui_parameters.display_y + 0.5 * used_scope->scope_render_data.gui_parameters.display_h;
    int scope_low_border_x_2 = used_scope->scope_render_data.gui_parameters.display_x + 0.5 * used_scope->scope_render_data.gui_parameters.display_w;
    int scope_low_border_y_2 = scope_low_border_y_1;

    int scope_left_border_x_1 = scope_low_border_x_1;
    int scope_left_border_y_1 = scope_low_border_y_1;
    int scope_left_border_x_2 = scope_low_border_x_1;
    int scope_left_border_y_2 = scope_low_border_y_1 - used_scope->scope_render_data.gui_parameters.display_h;

    int scope_top_border_x_1 = scope_low_border_x_1;
    int scope_top_border_y_1 = scope_left_border_y_2;
    int scope_top_border_x_2 = scope_low_border_x_2;
    int scope_top_border_y_2 = scope_left_border_y_2;

    int scope_right_border_x_1 = scope_top_border_x_2;
    int scope_right_border_y_1 = scope_low_border_y_2;
    int scope_right_border_x_2 = scope_top_border_x_2;
    int scope_right_border_y_2 = scope_top_border_y_2;

    
    // 8 горизонтальных линий + 16 вертикальных линий для сетки
    
    // H Line 1 - совпадает с нижней гранью дисплея - не выводим на рендер, но держим данные на всякий случай для ориентации

    used_scope->scope_render_data.gui_parameters.h_line_1_x1 = scope_low_border_x_1 + used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.gui_parameters.h_line_1_y1 = scope_low_border_y_1;

    used_scope->scope_render_data.gui_parameters.h_line_1_x2 = scope_low_border_x_2 - 1.2 * used_scope->scope_render_data.basic_border_thickness_2;

    used_scope->scope_render_data.gui_parameters.h_line_1_y2 = scope_low_border_y_2;

    used_scope->scope_render_data.gui_parameters.h_line_1_color = used_scope->scope_render_data.main_color_6;


    // H Line 2 - смещение на 1 margin вверх относительно 1 линии

    used_scope->scope_render_data.gui_parameters.h_line_2_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_2_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 1 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_2_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_2_y2 = used_scope->scope_render_data.gui_parameters.h_line_2_y1;

    used_scope->scope_render_data.gui_parameters.h_line_2_color = used_scope->scope_render_data.main_color_6;


    // H Line 3 - по аналогии

    used_scope->scope_render_data.gui_parameters.h_line_3_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_3_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 2 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_3_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_3_y2 = used_scope->scope_render_data.gui_parameters.h_line_3_y1;

    used_scope->scope_render_data.gui_parameters.h_line_3_color = used_scope->scope_render_data.main_color_6;


    // H Line 4

    used_scope->scope_render_data.gui_parameters.h_line_4_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_4_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 3 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_4_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_4_y2 = used_scope->scope_render_data.gui_parameters.h_line_4_y1;

    used_scope->scope_render_data.gui_parameters.h_line_4_color = used_scope->scope_render_data.main_color_6;


    // H Line 5

    used_scope->scope_render_data.gui_parameters.h_line_5_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_5_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 4 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_5_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_5_y2 = used_scope->scope_render_data.gui_parameters.h_line_5_y1;

    // Линия в центре - акцентная
    used_scope->scope_render_data.gui_parameters.h_line_5_color = used_scope->scope_render_data.main_color_4;


    // H Line 6

    used_scope->scope_render_data.gui_parameters.h_line_6_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_6_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 5 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_6_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_6_y2 = used_scope->scope_render_data.gui_parameters.h_line_6_y1;

    used_scope->scope_render_data.gui_parameters.h_line_6_color = used_scope->scope_render_data.main_color_6;


    // H Line 7

    used_scope->scope_render_data.gui_parameters.h_line_7_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_7_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 6 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_7_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_7_y2 = used_scope->scope_render_data.gui_parameters.h_line_7_y1;

    used_scope->scope_render_data.gui_parameters.h_line_7_color = used_scope->scope_render_data.main_color_6;


    // H Line 8

    used_scope->scope_render_data.gui_parameters.h_line_8_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_8_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 7 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_8_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_8_y2 = used_scope->scope_render_data.gui_parameters.h_line_8_y1;

    used_scope->scope_render_data.gui_parameters.h_line_8_color = used_scope->scope_render_data.main_color_6;


    // H Line 9

    used_scope->scope_render_data.gui_parameters.h_line_9_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_9_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 8 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_9_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_9_y2 = used_scope->scope_render_data.gui_parameters.h_line_9_y1;

    used_scope->scope_render_data.gui_parameters.h_line_9_color = used_scope->scope_render_data.main_color_6;


    // V Line 1

    used_scope->scope_render_data.gui_parameters.v_line_1_x1 = scope_left_border_x_1;
    used_scope->scope_render_data.gui_parameters.v_line_1_y1 = scope_left_border_y_1 - used_scope->scope_render_data.basic_border_thickness_2;

    used_scope->scope_render_data.gui_parameters.v_line_1_x2 = scope_left_border_x_2;
    used_scope->scope_render_data.gui_parameters.v_line_1_y2 = scope_left_border_y_2;

    used_scope->scope_render_data.gui_parameters.v_line_1_color = used_scope->scope_render_data.main_color_6;


    // V Line 2

    used_scope->scope_render_data.gui_parameters.v_line_2_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 1 * margin_in_pixels;
    used_scope->scope_render_data.gui_parameters.v_line_2_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_2_x2 = used_scope->scope_render_data.gui_parameters.v_line_2_x1;
    used_scope->scope_render_data.gui_parameters.v_line_2_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_2_color = used_scope->scope_render_data.main_color_6;


    // V Line 3
    
    used_scope->scope_render_data.gui_parameters.v_line_3_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 2 * margin_in_pixels; 
    
    used_scope->scope_render_data.gui_parameters.v_line_3_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_3_x2 = used_scope->scope_render_data.gui_parameters.v_line_3_x1;
    used_scope->scope_render_data.gui_parameters.v_line_3_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_3_color = used_scope->scope_render_data.main_color_6;


    // V Line 4

    used_scope->scope_render_data.gui_parameters.v_line_4_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 3 * margin_in_pixels;


    used_scope->scope_render_data.gui_parameters.v_line_4_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_4_x2 = used_scope->scope_render_data.gui_parameters.v_line_4_x1;
    used_scope->scope_render_data.gui_parameters.v_line_4_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_4_color = used_scope->scope_render_data.main_color_6;


    // V Line 5

    used_scope->scope_render_data.gui_parameters.v_line_5_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 4 * margin_in_pixels;


    used_scope->scope_render_data.gui_parameters.v_line_5_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_5_x2 = used_scope->scope_render_data.gui_parameters.v_line_5_x1;
    used_scope->scope_render_data.gui_parameters.v_line_5_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_5_color = used_scope->scope_render_data.main_color_6;


    // V Line 6

    used_scope->scope_render_data.gui_parameters.v_line_6_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 5 * margin_in_pixels;

    used_scope->scope_render_data.gui_parameters.v_line_6_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_6_x2 = used_scope->scope_render_data.gui_parameters.v_line_6_x1;
    used_scope->scope_render_data.gui_parameters.v_line_6_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_6_color = used_scope->scope_render_data.main_color_6;


    // V Line 7

    used_scope->scope_render_data.gui_parameters.v_line_7_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 6 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_7_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_7_x2 = used_scope->scope_render_data.gui_parameters.v_line_7_x1;
    used_scope->scope_render_data.gui_parameters.v_line_7_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_7_color = used_scope->scope_render_data.main_color_6;


    // V Line 8

    used_scope->scope_render_data.gui_parameters.v_line_8_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 7 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_8_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_8_x2 = used_scope->scope_render_data.gui_parameters.v_line_8_x1;
    used_scope->scope_render_data.gui_parameters.v_line_8_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_8_color = used_scope->scope_render_data.main_color_6;


    // V Line 9

    used_scope->scope_render_data.gui_parameters.v_line_9_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 8 * margin_in_pixels;
    
    // Выравнивание подписи
    used_scope->scope_render_data.scope_signature_textbox.x = used_scope->scope_render_data.gui_parameters.v_line_9_x1;
    
    used_scope->scope_render_data.gui_parameters.v_line_9_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_9_x2 = used_scope->scope_render_data.gui_parameters.v_line_9_x1;
    used_scope->scope_render_data.gui_parameters.v_line_9_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_9_color = used_scope->scope_render_data.main_color_6;


    // V Line 10

    used_scope->scope_render_data.gui_parameters.v_line_10_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 9 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_10_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_10_x2 = used_scope->scope_render_data.gui_parameters.v_line_10_x1;
    used_scope->scope_render_data.gui_parameters.v_line_10_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_10_color = used_scope->scope_render_data.main_color_6;


    // V Line 11

    used_scope->scope_render_data.gui_parameters.v_line_11_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 10 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_11_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_11_x2 = used_scope->scope_render_data.gui_parameters.v_line_11_x1;
    used_scope->scope_render_data.gui_parameters.v_line_11_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_11_color = used_scope->scope_render_data.main_color_6;


    // V Line 12

    used_scope->scope_render_data.gui_parameters.v_line_12_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 11 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_12_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_12_x2 = used_scope->scope_render_data.gui_parameters.v_line_12_x1;
    used_scope->scope_render_data.gui_parameters.v_line_12_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_12_color = used_scope->scope_render_data.main_color_6;


    // V Line 13

    used_scope->scope_render_data.gui_parameters.v_line_13_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 12 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_13_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_13_x2 = used_scope->scope_render_data.gui_parameters.v_line_13_x1;
    used_scope->scope_render_data.gui_parameters.v_line_13_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_13_color = used_scope->scope_render_data.main_color_6;


    // V Line 14

    used_scope->scope_render_data.gui_parameters.v_line_14_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 13 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_14_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_14_x2 = used_scope->scope_render_data.gui_parameters.v_line_14_x1;
    used_scope->scope_render_data.gui_parameters.v_line_14_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_14_color = used_scope->scope_render_data.main_color_6;


    // V Line 15

    used_scope->scope_render_data.gui_parameters.v_line_15_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 14 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_15_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_15_x2 = used_scope->scope_render_data.gui_parameters.v_line_15_x1;
    used_scope->scope_render_data.gui_parameters.v_line_15_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_15_color = used_scope->scope_render_data.main_color_6;


    // V Line 16

    used_scope->scope_render_data.gui_parameters.v_line_16_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 15 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_16_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_16_x2 = used_scope->scope_render_data.gui_parameters.v_line_16_x1;
    used_scope->scope_render_data.gui_parameters.v_line_16_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_16_color = used_scope->scope_render_data.main_color_6;


    // V Line 17

    used_scope->scope_render_data.gui_parameters.v_line_17_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 16 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_17_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_17_x2 = used_scope->scope_render_data.gui_parameters.v_line_17_x1;
    used_scope->scope_render_data.gui_parameters.v_line_17_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_17_color = used_scope->scope_render_data.main_color_6;


    // ===== Кнопки и информация о кнопках =====

    // Общий цвет для всех пояснений
    used_scope->scope_render_data.gui_parameters.description_text_color = used_scope->scope_render_data.main_color_2;

    
    /*
        Базируемся по направлению

            scope_right_border_x_2
            scope_right_border_y_2

        через

            buttons_signature_width_units;
            buttons_signature_height_units;

            buttons_1_width_units;
            buttons_1_height_units;

            buttons_2_width_units;
            buttons_2_height_units;

    */
    
    // ===== Прямоугольник пояснения и координаты для текстбокса инструкции по изменению масштаба сигнала =====

    int signatures_width = used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * used_scope->scope_render_data.gui_parameters.buttons_signature_width_units;
    int signatures_height = used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * used_scope->scope_render_data.gui_parameters.buttons_signature_height_units;

    int buttons_1_width = used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * used_scope->scope_render_data.gui_parameters.buttons_1_width_units;
    int buttons_1_height = used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * used_scope->scope_render_data.gui_parameters.buttons_1_height_units;


    // Один отступ + половина размера
    used_scope->scope_render_data.gui_parameters.value_scale_set_info_x_1 = 
        scope_right_border_x_2 + margin_in_pixels + signatures_width * 0.5;
    
    used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 = 
        scope_right_border_y_2 + signatures_height * 0.5;

    used_scope->scope_render_data.gui_parameters.value_scale_set_info_w_1 = signatures_width; 

    used_scope->scope_render_data.gui_parameters.value_scale_set_info_h_1 = signatures_height;

    used_scope->scope_render_data.gui_parameters.value_scale_set_info_fill_color_1 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.value_scale_set_info_border_color_1 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.value_scale_set_info_border_thickness_1 = used_scope->scope_render_data.basic_border_thickness_2;


    // Текстбокс пояснения
    used_scope->scope_render_data.change_value_scale_instruction_textbox = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_value_scale_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.value_scale_set_info_x_1;
    used_scope->scope_render_data.change_value_scale_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_value_scale_instruction_textbox, "VS");

    // Уменьшение

    used_scope->scope_render_data.decrease_value_scale_button.x = 
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_x_1 + signatures_width * 0.5 + buttons_1_width * 0.5;

    used_scope->scope_render_data.decrease_value_scale_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1;
    
    used_scope->scope_render_data.decrease_value_scale_button.w = buttons_1_width;
    used_scope->scope_render_data.decrease_value_scale_button.h = buttons_1_height;
    used_scope->scope_render_data.decrease_value_scale_button.radius = 0;
    used_scope->scope_render_data.decrease_value_scale_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.decrease_value_scale_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.decrease_value_scale_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.decrease_value_scale_button.pressed_color = used_scope->scope_render_data.main_color_4;
    used_scope->scope_render_data.decrease_value_scale_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.decrease_value_scale_button.down_inside = false;
    used_scope->scope_render_data.decrease_value_scale_button.on_click = decrease_scope_value_scale;

    
    used_scope->scope_render_data.decrease_value_scale_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_value_scale_button.button_text.x = used_scope->scope_render_data.decrease_value_scale_button.x;
    used_scope->scope_render_data.decrease_value_scale_button.button_text.y = used_scope->scope_render_data.decrease_value_scale_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_value_scale_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_value_scale_button.x = 
        used_scope->scope_render_data.decrease_value_scale_button.x + buttons_1_width * 1;

    used_scope->scope_render_data.increase_value_scale_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1;
    
    used_scope->scope_render_data.increase_value_scale_button.w = buttons_1_width;
    used_scope->scope_render_data.increase_value_scale_button.h = buttons_1_height;
    used_scope->scope_render_data.increase_value_scale_button.radius = 0;
    used_scope->scope_render_data.increase_value_scale_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.increase_value_scale_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.increase_value_scale_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.increase_value_scale_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.increase_value_scale_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.increase_value_scale_button.down_inside = false;
    used_scope->scope_render_data.increase_value_scale_button.on_click = increase_scope_value_scale;

    
    used_scope->scope_render_data.increase_value_scale_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.increase_value_scale_button.button_text.x = used_scope->scope_render_data.increase_value_scale_button.x;
    used_scope->scope_render_data.increase_value_scale_button.button_text.y = used_scope->scope_render_data.increase_value_scale_button.y;

    Textbox_set_content(&used_scope->scope_render_data.increase_value_scale_button.button_text, "+");


    // ===== Прямоугольник пояснения и координаты для текстбокса инструкции по изменению масштаба времени =====


    // Один отступ + половина размера
    used_scope->scope_render_data.gui_parameters.time_scale_set_info_x_1 = 
        scope_right_border_x_2 + margin_in_pixels + signatures_width * 0.5;
    
    used_scope->scope_render_data.gui_parameters.time_scale_set_info_y_1 = 
        scope_right_border_y_2 + signatures_height * 1.5;

    used_scope->scope_render_data.gui_parameters.time_scale_set_info_w_1 = signatures_width; 

    used_scope->scope_render_data.gui_parameters.time_scale_set_info_h_1 = signatures_height;

    used_scope->scope_render_data.gui_parameters.time_scale_set_info_fill_color_1 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.time_scale_set_info_border_color_1 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.time_scale_set_info_border_thickness_1 = used_scope->scope_render_data.basic_border_thickness_2;


    // Текстбокс пояснения
    used_scope->scope_render_data.change_time_scale_instruction_textbox = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_time_scale_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.time_scale_set_info_x_1;
    used_scope->scope_render_data.change_time_scale_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.time_scale_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_time_scale_instruction_textbox, "TS");


    // Уменьшение

    used_scope->scope_render_data.decrease_time_scale_button.x = 
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_x_1 + signatures_width * 0.5 + buttons_1_width * 0.5;

    used_scope->scope_render_data.decrease_time_scale_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 + buttons_1_height;
    
    used_scope->scope_render_data.decrease_time_scale_button.w = buttons_1_width;
    used_scope->scope_render_data.decrease_time_scale_button.h = buttons_1_height;
    used_scope->scope_render_data.decrease_time_scale_button.radius = 0;
    used_scope->scope_render_data.decrease_time_scale_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.decrease_time_scale_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.decrease_time_scale_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.decrease_time_scale_button.pressed_color = used_scope->scope_render_data.main_color_4;
    used_scope->scope_render_data.decrease_time_scale_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.decrease_time_scale_button.down_inside = false;
    used_scope->scope_render_data.decrease_time_scale_button.on_click = decrease_scope_time_scale;

    
    used_scope->scope_render_data.decrease_time_scale_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_time_scale_button.button_text.x = used_scope->scope_render_data.decrease_time_scale_button.x;
    used_scope->scope_render_data.decrease_time_scale_button.button_text.y = used_scope->scope_render_data.decrease_time_scale_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_time_scale_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_time_scale_button.x = 
        used_scope->scope_render_data.decrease_time_scale_button.x + buttons_1_width * 1;

    used_scope->scope_render_data.increase_time_scale_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 + buttons_1_height;
    
    used_scope->scope_render_data.increase_time_scale_button.w = buttons_1_width;
    used_scope->scope_render_data.increase_time_scale_button.h = buttons_1_height;
    used_scope->scope_render_data.increase_time_scale_button.radius = 0;
    used_scope->scope_render_data.increase_time_scale_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.increase_time_scale_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.increase_time_scale_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.increase_time_scale_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.increase_time_scale_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.increase_time_scale_button.down_inside = false;
    used_scope->scope_render_data.increase_time_scale_button.on_click = increase_scope_time_scale;

    
    used_scope->scope_render_data.increase_time_scale_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.increase_time_scale_button.button_text.x = used_scope->scope_render_data.increase_time_scale_button.x;
    used_scope->scope_render_data.increase_time_scale_button.button_text.y = used_scope->scope_render_data.increase_time_scale_button.y;

    Textbox_set_content(&used_scope->scope_render_data.increase_time_scale_button.button_text, "+");


    // ===== Прямоугольник пояснения и координаты для текстбокса инструкции по изменению амплитуды сигнала

    // Один отступ + половина размера
    used_scope->scope_render_data.gui_parameters.amplitude_set_info_x_1 = 
        scope_right_border_x_2 + margin_in_pixels + signatures_width * 0.5;
    
    used_scope->scope_render_data.gui_parameters.amplitude_set_info_y_1 = 
        scope_right_border_y_2 + signatures_height * 3.5;

    used_scope->scope_render_data.gui_parameters.amplitude_set_info_w_1 = signatures_width; 

    used_scope->scope_render_data.gui_parameters.amplitude_set_info_h_1 = signatures_height;

    used_scope->scope_render_data.gui_parameters.amplitude_set_info_fill_color_1 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.amplitude_set_info_border_color_1 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.amplitude_set_info_border_thickness_1 = used_scope->scope_render_data.basic_border_thickness_2;


    // Текстбокс пояснения
    used_scope->scope_render_data.change_amplitude_instruction_textbox = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_amplitude_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.amplitude_set_info_x_1;
    used_scope->scope_render_data.change_amplitude_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.amplitude_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_amplitude_instruction_textbox, "AC");


    // Уменьшение

    used_scope->scope_render_data.decrease_amplitude_button.x = 
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_x_1 + signatures_width * 0.5 + buttons_1_width * 0.5;

    used_scope->scope_render_data.decrease_amplitude_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 + 3 * buttons_1_height;
    
    used_scope->scope_render_data.decrease_amplitude_button.w = buttons_1_width;
    used_scope->scope_render_data.decrease_amplitude_button.h = buttons_1_height;
    used_scope->scope_render_data.decrease_amplitude_button.radius = 0;
    used_scope->scope_render_data.decrease_amplitude_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.decrease_amplitude_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.decrease_amplitude_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.decrease_amplitude_button.pressed_color = used_scope->scope_render_data.main_color_4;
    used_scope->scope_render_data.decrease_amplitude_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.decrease_amplitude_button.down_inside = false;
    used_scope->scope_render_data.decrease_amplitude_button.on_click = decrease_amplitude;

    
    used_scope->scope_render_data.decrease_amplitude_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_amplitude_button.button_text.x = used_scope->scope_render_data.decrease_amplitude_button.x;
    used_scope->scope_render_data.decrease_amplitude_button.button_text.y = used_scope->scope_render_data.decrease_amplitude_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_amplitude_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_amplitude_button.x = 
        used_scope->scope_render_data.decrease_amplitude_button.x + buttons_1_width * 1;

    used_scope->scope_render_data.increase_amplitude_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 + 3 * buttons_1_height;
    
    used_scope->scope_render_data.increase_amplitude_button.w = buttons_1_width;
    used_scope->scope_render_data.increase_amplitude_button.h = buttons_1_height;
    used_scope->scope_render_data.increase_amplitude_button.radius = 0;
    used_scope->scope_render_data.increase_amplitude_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.increase_amplitude_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.increase_amplitude_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.increase_amplitude_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.increase_amplitude_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.increase_amplitude_button.down_inside = false;
    used_scope->scope_render_data.increase_amplitude_button.on_click = increase_amplitude;

    
    used_scope->scope_render_data.increase_amplitude_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.increase_amplitude_button.button_text.x = used_scope->scope_render_data.increase_amplitude_button.x;
    used_scope->scope_render_data.increase_amplitude_button.button_text.y = used_scope->scope_render_data.increase_amplitude_button.y;

    Textbox_set_content(&used_scope->scope_render_data.increase_amplitude_button.button_text, "+");


    // ===== Прямоугольник пояснения и координаты для текстбокса инструкции по изменению частоты сигнала

    // Один отступ + половина размера
    used_scope->scope_render_data.gui_parameters.frequency_set_info_x_1 = 
        scope_right_border_x_2 + margin_in_pixels + signatures_width * 0.5;
    
    used_scope->scope_render_data.gui_parameters.frequency_set_info_y_1 = 
        scope_right_border_y_2 + signatures_height * 4.5;

    used_scope->scope_render_data.gui_parameters.frequency_set_info_w_1 = signatures_width; 

    used_scope->scope_render_data.gui_parameters.frequency_set_info_h_1 = signatures_height;

    used_scope->scope_render_data.gui_parameters.frequency_set_info_fill_color_1 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.frequency_set_info_border_color_1 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.frequency_set_info_border_thickness_1 = used_scope->scope_render_data.basic_border_thickness_2;


    // Текстбокс пояснения
    used_scope->scope_render_data.change_frequency_instruction_textbox = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_frequency_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.frequency_set_info_x_1;
    used_scope->scope_render_data.change_frequency_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.frequency_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_frequency_instruction_textbox, "FC");


    // Уменьшение

    used_scope->scope_render_data.decrease_frequency_button.x = 
        used_scope->scope_render_data.gui_parameters.frequency_set_info_x_1 + signatures_width * 0.5 + buttons_1_width * 0.5;

    used_scope->scope_render_data.decrease_frequency_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 + 4 * buttons_1_height;
    
    used_scope->scope_render_data.decrease_frequency_button.w = buttons_1_width;
    used_scope->scope_render_data.decrease_frequency_button.h = buttons_1_height;
    used_scope->scope_render_data.decrease_frequency_button.radius = 0;
    used_scope->scope_render_data.decrease_frequency_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.decrease_frequency_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.decrease_frequency_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.decrease_frequency_button.pressed_color = used_scope->scope_render_data.main_color_4;
    used_scope->scope_render_data.decrease_frequency_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.decrease_frequency_button.down_inside = false;
    used_scope->scope_render_data.decrease_frequency_button.on_click = decrease_frequency;

    
    used_scope->scope_render_data.decrease_frequency_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_frequency_button.button_text.x = used_scope->scope_render_data.decrease_frequency_button.x;
    used_scope->scope_render_data.decrease_frequency_button.button_text.y = used_scope->scope_render_data.decrease_frequency_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_frequency_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_frequency_button.x = 
        used_scope->scope_render_data.decrease_frequency_button.x + buttons_1_width * 1;

    used_scope->scope_render_data.increase_frequency_button.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1 + 4 * buttons_1_height;
    
    used_scope->scope_render_data.increase_frequency_button.w = buttons_1_width;
    used_scope->scope_render_data.increase_frequency_button.h = buttons_1_height;
    used_scope->scope_render_data.increase_frequency_button.radius = 0;
    used_scope->scope_render_data.increase_frequency_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.increase_frequency_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.increase_frequency_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.increase_frequency_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.increase_frequency_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.increase_frequency_button.down_inside = false;
    used_scope->scope_render_data.increase_frequency_button.on_click = increase_frequency;

    
    used_scope->scope_render_data.increase_frequency_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.increase_frequency_button.button_text.x = used_scope->scope_render_data.increase_frequency_button.x;
    used_scope->scope_render_data.increase_frequency_button.button_text.y = used_scope->scope_render_data.increase_frequency_button.y;

    Textbox_set_content(&used_scope->scope_render_data.increase_frequency_button.button_text, "+");


    
    // Смена сигнала

    used_scope->scope_render_data.signal_change_button.x = 
        used_scope->scope_render_data.decrease_frequency_button.x - buttons_1_width * 1;

    used_scope->scope_render_data.signal_change_button.y = used_scope->scope_render_data.increase_frequency_button.button_text.y + 2.5 * buttons_1_height;
    
    used_scope->scope_render_data.signal_change_button.w = buttons_1_width;
    used_scope->scope_render_data.signal_change_button.h = buttons_1_height * 2;
    used_scope->scope_render_data.signal_change_button.radius = 0;
    used_scope->scope_render_data.signal_change_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.signal_change_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.signal_change_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.signal_change_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.signal_change_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.signal_change_button.down_inside = false;
    used_scope->scope_render_data.signal_change_button.on_click = change_controlled_signal;

    
    used_scope->scope_render_data.signal_change_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.signal_change_button.button_text.x = used_scope->scope_render_data.signal_change_button.x;
    used_scope->scope_render_data.signal_change_button.button_text.y = used_scope->scope_render_data.signal_change_button.y;
        
    Textbox_set_draw_mode(&used_scope->scope_render_data.signal_change_button.button_text, VERTICAL_TEXT);
    Textbox_set_content(&used_scope->scope_render_data.signal_change_button.button_text, "SIGNAL");



    // Смена режима

    used_scope->scope_render_data.mode_change_button.x = 
        used_scope->scope_render_data.decrease_frequency_button.x - buttons_1_width * 0;

    used_scope->scope_render_data.mode_change_button.y = used_scope->scope_render_data.increase_frequency_button.button_text.y + 2.5 * buttons_1_height;
    
    used_scope->scope_render_data.mode_change_button.w = buttons_1_width;
    used_scope->scope_render_data.mode_change_button.h = buttons_1_height * 2;
    used_scope->scope_render_data.mode_change_button.radius = 0;
    used_scope->scope_render_data.mode_change_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.mode_change_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.mode_change_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.mode_change_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.mode_change_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.mode_change_button.down_inside = false;
    used_scope->scope_render_data.mode_change_button.on_click = change_scope_render_mode;

    
    used_scope->scope_render_data.mode_change_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.mode_change_button.button_text.x = used_scope->scope_render_data.mode_change_button.x;
    used_scope->scope_render_data.mode_change_button.button_text.y = used_scope->scope_render_data.mode_change_button.y;
        
    Textbox_set_draw_mode(&used_scope->scope_render_data.mode_change_button.button_text, VERTICAL_TEXT);
    Textbox_set_content(&used_scope->scope_render_data.mode_change_button.button_text, "MODE");


    // Проигрывание сигнала

    used_scope->scope_render_data.controlled_signal_play_button.x = 
        used_scope->scope_render_data.decrease_frequency_button.x + buttons_1_width * 1;

    used_scope->scope_render_data.controlled_signal_play_button.y = used_scope->scope_render_data.increase_frequency_button.button_text.y + 2.5 * buttons_1_height;
    
    used_scope->scope_render_data.controlled_signal_play_button.w = buttons_1_width;
    used_scope->scope_render_data.controlled_signal_play_button.h = buttons_1_height * 2;
    used_scope->scope_render_data.controlled_signal_play_button.radius = 0;
    used_scope->scope_render_data.controlled_signal_play_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.controlled_signal_play_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.controlled_signal_play_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.controlled_signal_play_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.controlled_signal_play_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.controlled_signal_play_button.down_inside = false;
    used_scope->scope_render_data.controlled_signal_play_button.on_click = play_controlled_signal;

    
    used_scope->scope_render_data.controlled_signal_play_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.controlled_signal_play_button.button_text.x = used_scope->scope_render_data.controlled_signal_play_button.x;
    used_scope->scope_render_data.controlled_signal_play_button.button_text.y = used_scope->scope_render_data.controlled_signal_play_button.y;
        
    Textbox_set_draw_mode(&used_scope->scope_render_data.controlled_signal_play_button.button_text, VERTICAL_TEXT);
    Textbox_set_content(&used_scope->scope_render_data.controlled_signal_play_button.button_text, "PLAY");


    // Включение-выключение осциллографа 

    used_scope->scope_render_data.scope_on_off_button.x = 
        used_scope->scope_render_data.mode_change_button.x;

    used_scope->scope_render_data.scope_on_off_button.y = used_scope->scope_render_data.increase_frequency_button.button_text.y + 5 * buttons_1_height;
    
    used_scope->scope_render_data.scope_on_off_button.w = buttons_1_width * 3;
    used_scope->scope_render_data.scope_on_off_button.h = buttons_1_height * 2;
    used_scope->scope_render_data.scope_on_off_button.radius = 0;
    used_scope->scope_render_data.scope_on_off_button.border_thickness = used_scope->scope_render_data.basic_border_thickness_2;
    used_scope->scope_render_data.scope_on_off_button.idle_color = used_scope->scope_render_data.main_color_2;
    used_scope->scope_render_data.scope_on_off_button.hover_color = hex_to_sdl_color("#1a2209", 255);
    used_scope->scope_render_data.scope_on_off_button.pressed_color = used_scope->scope_render_data.main_color_5;
    used_scope->scope_render_data.scope_on_off_button.border_color = used_scope->scope_render_data.main_color_1;
    used_scope->scope_render_data.scope_on_off_button.down_inside = false;

    // Передаём ссылку на осциллограф
    used_scope->scope_render_data.scope_on_off_button.user_data = used_scope;
    used_scope->scope_render_data.scope_on_off_button.on_click = on_off_command;

    
    used_scope->scope_render_data.scope_on_off_button.button_text = 
        *Textbox_init(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.scope_on_off_button.button_text.x = used_scope->scope_render_data.scope_on_off_button.x;
    used_scope->scope_render_data.scope_on_off_button.button_text.y = used_scope->scope_render_data.scope_on_off_button.y;
        
    Textbox_set_content(&used_scope->scope_render_data.scope_on_off_button.button_text, "ON / OFF");

        
    // Второй дисплей для информации
    used_scope->scope_render_data.gui_parameters.scope_info_zone_display_x_1 = used_scope->scope_render_data.gui_parameters.display_x;
    used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1 = scope_low_border_y_1 + used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 1.5;

    used_scope->scope_render_data.gui_parameters.scope_info_zone_display_w_1 = used_scope->scope_render_data.gui_parameters.display_w;

    used_scope->scope_render_data.gui_parameters.scope_info_zone_display_h_1 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit - used_scope->scope_render_data.basic_border_thickness_2) * 2;


    // 4 текстбокса на втором дисплее

    // Масштаб значения сигнала

    // По 3 линии
    used_scope->scope_render_data.gui_parameters.signal_scale_info_x_1 = used_scope->scope_render_data.gui_parameters.v_line_3_x1;
    used_scope->scope_render_data.gui_parameters.signal_scale_info_y_1 = used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1;

    used_scope->scope_render_data.gui_parameters.signal_scale_info_fill_color_1 = used_scope->scope_render_data.main_color_5;

    used_scope->scope_render_data.signal_scale_textbox = 
        *Textbox_init(used_scope->scope_render_data.gui_parameters.signal_scale_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.signal_scale_textbox.x = used_scope->scope_render_data.gui_parameters.signal_scale_info_x_1;
    used_scope->scope_render_data.signal_scale_textbox.y = used_scope->scope_render_data.gui_parameters.signal_scale_info_y_1;

    // Init значение текста - поменяется сразу при обновлении буфера
    Textbox_set_content(&used_scope->scope_render_data.signal_scale_textbox, "S_SCALE");


    // Масштаб времени

    // По 7 линии
    used_scope->scope_render_data.gui_parameters.time_scale_info_x_1 = used_scope->scope_render_data.gui_parameters.v_line_7_x1;
    used_scope->scope_render_data.gui_parameters.time_scale_info_y_1 = used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1;

    used_scope->scope_render_data.gui_parameters.time_scale_info_fill_color_1 = used_scope->scope_render_data.main_color_4;

    used_scope->scope_render_data.time_scale_textbox = 
        *Textbox_init(used_scope->scope_render_data.gui_parameters.time_scale_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.time_scale_textbox.x = used_scope->scope_render_data.gui_parameters.time_scale_info_x_1;
    used_scope->scope_render_data.time_scale_textbox.y = used_scope->scope_render_data.gui_parameters.time_scale_info_y_1;

    // Init значение текста - поменяется сразу при обновлении буфера
    Textbox_set_content(&used_scope->scope_render_data.time_scale_textbox, "T_SCALE");


    // Амплитуда

    // По 11 линии
    used_scope->scope_render_data.gui_parameters.amplitude_info_x_1 = used_scope->scope_render_data.gui_parameters.v_line_11_x1;
    used_scope->scope_render_data.gui_parameters.amplitude_info_y_1 = used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1;

    used_scope->scope_render_data.gui_parameters.amplitude_info_fill_color_1 = used_scope->scope_render_data.main_color_5;

    used_scope->scope_render_data.amplitude_textbox = 
        *Textbox_init(used_scope->scope_render_data.gui_parameters.amplitude_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.amplitude_textbox.x = used_scope->scope_render_data.gui_parameters.amplitude_info_x_1;
    used_scope->scope_render_data.amplitude_textbox.y = used_scope->scope_render_data.gui_parameters.amplitude_info_y_1;

    // Init значение текста - поменяется сразу при обновлении буфера
    Textbox_set_content(&used_scope->scope_render_data.amplitude_textbox, "AMPLITUDE");


    // Частота

    // По 15 линии
    used_scope->scope_render_data.gui_parameters.frequency_or_period_info_x_1 = used_scope->scope_render_data.gui_parameters.v_line_15_x1;
    used_scope->scope_render_data.gui_parameters.frequency_or_period_info_y_1 = used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1;

    used_scope->scope_render_data.gui_parameters.frequency_or_period_info_fill_color_1 = used_scope->scope_render_data.main_color_4;

    used_scope->scope_render_data.frequency_or_period_textbox = 
        *Textbox_init(used_scope->scope_render_data.gui_parameters.frequency_or_period_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.frequency_or_period_textbox.x = used_scope->scope_render_data.gui_parameters.frequency_or_period_info_x_1;
    used_scope->scope_render_data.frequency_or_period_textbox.y = used_scope->scope_render_data.gui_parameters.frequency_or_period_info_y_1;

    // Init значение текста - поменяется сразу при обновлении буфера
    Textbox_set_content(&used_scope->scope_render_data.frequency_or_period_textbox, "FREQUENCY");
}




void scope_screens_gui_renew_by_signal_data(Scope* used_scope)
{
    // Апдейт структуры скрина, исходя из флагов состояния осциллографа.

    /*
    
        Задача функции - зная:

            Текущий режим демонстрации сигнала: used_scope->main_settings.current_mode

            Текущую единицу сетки по оси y (единица значения сигнала): used_scope->main_settings.current_signal_units; (ВЛИЯЕТ ЛИШЬ НА РЕНДЕР СИГНАЛА, НЕ ПОКАЗАТЕЛЕЙ, переменная всегда в Вольтах)
            Текущую единицу сетки по оси x (единица времени сигнала): used_scope->main_settings.current_time_units; (ВЛИЯЕТ ЛИШЬ НА РЕНДЕР СИГНАЛА, НЕ ПОКАЗАТЕЛЕЙ, переменная всегда в секундах)
            Текущую единицу отображения частоты: used_scope->main_settings.current_frequency_units (ВЛИЯЕТ ЛИШЬ НА РЕНДЕР СИГНАЛА, НЕ ПОКАЗАТЕЛЕЙ, переменная всегда в Герцах)

            Количество периодов для демонстрации: used_scope->main_settings.periods_to_display

            Буфер: used_scope->signal_control_data.scope_buffer_data;               (времена в секундах, сигнал в Вольтах)
            Период: used_scope->signal_control_data.current_period_value;           (в секундах)
            Частоту: used_scope->signal_control_data.current_frequency_value;       (в Герцах)
            Амплитуду:  used_scope->signal_control_data.current_max_signal_value    (В Вольтах)


            Координаты центра дисплея и его размеры (для заполнения used_scope->scope_render_data.signal_render_data):

            used_scope->scope_render_data.gui_parameters.display_x, 
            used_scope->scope_render_data.gui_parameters.display_y,
            used_scope->scope_render_data.gui_parameters.display_w,
            used_scope->scope_render_data.gui_parameters.display_h,
            

        Проделать такое:

            1) Исходя из значений периода, частоты и аплитуды - понять в каких единицах их надо записывать в текстбоксы в формате "xxx.xxx UNIT" - допустим 999.930 Гц 1.010 Khz - 
            допустимо, а 1002.332 Гц - недопустимо (так же с вольтами). Записать локальные переменные в этой функции, которыми можно будет воспользоваться позже для вывода амплитуды,
            периода / частоты. + Надо понять частоту или период в Textbox_set_content(&used_scope->scope_render_data.frequency_or_period_textbox, "СТРОКА"); и
            временную единицу или количество периодов в Textbox_set_content(&used_scope->scope_render_data.time_scale_textbox, "СТРОКА") мы демонстрируем в строках, исходя из used_scope->main_settings.current_mode
            (всё, что связано с периодом показывается только при SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM)


            2) Обновить строки контента текстбоксов по формату подбираемому расчётами и информацией из данных:

                1.1 Textbox_set_content(&used_scope->scope_render_data.signal_scale_textbox, "СТРОКА"); - Если used_scope->main_settings.current_signal_units == VOLTS_SU выставить "SU: V."

                1.2 Textbox_set_content(&used_scope->scope_render_data.time_scale_textbox, "СТРОКА"); - По time_units выставитьь "TU: MS" или "TU: S" или "TU: NS."... или выставить points_to_draw_quantity Per.

                1.3 Textbox_set_content(&used_scope->scope_render_data.amplitude_textbox, "СТРОКА"); По used_scope->signal_control_data.current_max_signal_value выставить "MAX: points_to_draw_quantity V."

                1.4 Textbox_set_content(&used_scope->scope_render_data.frequency_or_period_textbox, "СТРОКА");  - Тут в зависимости от режима used_scope->main_settings.current_mode либо
                выставить текущую частоту, как "F: points_to_draw_quantity FREQ_UNITS" "T: points_to_draw_quantity TIME_UNITS"       
                                                  

            3) Подготовить used_scope->scope_render_data.signal_render_data, исходя из used_scope->main_settings.current_mode и других параметров:

                3.0. проверяемся на ограничение по частоте used_scope->signal_control_data.current_frequency_value для данного режима (на какой-то частоте какой-то режим нереален -
                на малой мы можем работать только по по 3.1 и 3.2, на большой можем работать только по 3.1. и 3.3, соотв. этот шаг штука ещё и определяет допустимые переходы
                в режимах рендеринга - через сет used_scope->main_settings.acessable_modes: 
                
                used_scope->main_settings.acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
                used_scope->main_settings.acessable_modes[1] = SCOPE_MODE_SCROLL_TO_RIGHT_SRM;
                used_scope->main_settings.acessable_modes[2] = LIMIT_SRM;

                или

                used_scope->main_settings.acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
                used_scope->main_settings.acessable_modes[1] = SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM
                used_scope->main_settings.acessable_modes[2] = LIMIT_SRM;

                которые далее будут использованы на коллбеке кнопки смены режима для цикличной смены режима отображения в used_scope->main_settings.current_mode, по которому
                пойдут шаги 3.1 - 3.3

                3.1 При SCOPE_MODE_FIXED_TIME_STEP_SRM: смотрим на used_scope->main_settings.current_signal_units и used_scope->main_settings.current_time_units, смотрим на
                used_scope->signal_control_data.scope_buffer_data и исходя из структуры used_scope->scope_render_data.signal_render_data считаем нужное количество точек для рендера на весь дисплей,
                забираем из даты буффера это количество точек количество точек, сдвигаясь от head на рассчётное количество, игнорируя пустые и забивая used_scope->signal_control_data.scope_buffer_data
                в порядке, соответвтующем порядку used_scope->signal_control_data.scope_buffer_data, от элемента head - elements_quantity (или от последнего заполненного, не трогая в 
                used_scope->scope_render_data.signal_render_data клетки, соответствующие незаполненным в данный момент внутри used_scope->signal_control_data.scope_buffer_data).

                3.2 При SCOPE_MODE_SCROLL_TO_RIGHT_SRM ,при соответствии частоты просто начинаем забивать head used_scope->main_settings.current_signal_units c [0] элемента
                used_scope->scope_render_data.signal_render_data, каждый шаг сдвигая элементы used_scope->scope_render_data.signal_render_data вправо, а последний обновляя текущим head, 
                при выходе данных за конец used_scope->scope_render_data.signal_render_data данное из [limit] выбрасывается и сдвиги продолжаются по новым head head used_scope->main_settings.current_signal_units

                3.3 При SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM, по значениям used_scope->signal_control_data.current_period_value и used_scope->main_settings.periods_to_display, определить,
                какой кусок used_scope->signal_control_data.scope_buffer_data надо забрать, начиная с первого после head фильтрованного перехода сигнала через 0, забрать этот кусок 
                и вставить в used_scope->scope_render_data.signal_render_data, в соотвт. с текущим масштабом развертки на период и значение. 
    */

    if (!used_scope) return;

    scope_signal_control_ctx* signal = &used_scope->signal_control_data;
    scope_render_ctx* render = &used_scope->scope_render_data;
    scope_main_settings_ctx* ms = &used_scope->main_settings;


    // =========================================================
    // 1. Нормализация значений (UI слой)
    // =========================================================

    const char* freq_unit;
    float freq = format_frequency(signal->measured_signal_characteristics.measured_frequency, &freq_unit);

    const char* time_unit;

    float period = format_period(signal->measured_signal_characteristics.measured_period, &time_unit);
    format_time(used_scope, &time_unit);

    float time_scale = (float)ms->time_val_in_one_unit;

    const char* volt_unit;
    float max_v = format_voltage(signal->measured_signal_characteristics.measured_max, &volt_unit);

    const char* amp_unit;
    float amp = format_voltage(signal->measured_signal_characteristics.measured_amplitude, &amp_unit);


    // =========================================================
    // 2. Текстбоксы (UI слой)
    // =========================================================

    // Единицы
    if (ms->current_signal_units == VOLTS_SU)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "SU: %.3f %s", max_v, volt_unit);
        Textbox_set_content(&render->signal_scale_textbox, buf);
    }

    // Время
    if (ms->current_mode != SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "ST: %.3f %s", time_scale, time_unit);
        Textbox_set_content(&render->time_scale_textbox, buf);
    }
    else
    {
        // фиксированное окно времени
        char buf[64];
        snprintf(buf, sizeof(buf), "%d P", ms->periods_to_display);
        Textbox_set_content(&render->time_scale_textbox, buf);
    }

    // Амплитуда
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "AMP: %.3f V", amp);
        Textbox_set_content(&render->amplitude_textbox, buf);
    }


    // Частота или приод

    {
        char buf[64];

        if (ms->current_mode == SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM)
        {
            snprintf(buf, sizeof(buf), "T: %.3f %s", period, time_unit);
        }
        else
        {
            snprintf(buf, sizeof(buf), "F: %.3f %s", freq, freq_unit);
        }

        Textbox_set_content(&render->frequency_or_period_textbox, buf);
    }


    // =========================================================
    // 3. Рендер дата сигнала по текущим режмам и текущему буфферу (UI слой)
    // =========================================================

    /*

        scope_screens_gui_renew_by_signal_data()

        ├── 1. Проверка частоты
        ├── 2. Выбор режима
        │       ├── SCOPE_MODE_FIXED_TIME_STEP_SRM
        │       ├── SCOPE_MODE_SCROLL_TO_RIGHT_SRM
        │       └── SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM
        │
        ├── 3. заполнение signal_render_data
        └── 4. обновление UI текстбоксов

    */


    // ===== Проверка частоты ===== 

    float f = used_scope->signal_control_data.measured_signal_characteristics.measured_frequency;


    // Очистка
    ms->acessable_modes[0] = LIMIT_SRM;
    ms->acessable_modes[1] = LIMIT_SRM;
    ms->acessable_modes[2] = LIMIT_SRM;

    if (f <= FREQ_TO_SEPARATE_MODES_LB)
    {
        ms->acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
        ms->acessable_modes[1] = SCOPE_MODE_SCROLL_TO_RIGHT_SRM;
        ms->acessable_modes[2] = LIMIT_SRM;
    }
    
    if (f >= FREQ_TO_SEPARATE_MODES_HB)
    {
        ms->acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
        ms->acessable_modes[1] = SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM;
        ms->acessable_modes[2] = LIMIT_SRM;
    }

    // В промежутках останутся старые режимы


    bool safe_old_mode = false;

    for (int i = 0; i <= 2; i++)
    {
        if (ms->current_mode == ms->acessable_modes[i]) safe_old_mode = true;
    }

    if (!safe_old_mode) ms->current_mode = ms->acessable_modes[0];


    /*

    // ===== Присвоение данных о сигнале для рендера ====

    signal_render_ctx* signal_render_data_out = &render->signal_render_data;

    switch (ms->current_mode)
    {
        case SCOPE_MODE_FIXED_TIME_STEP_SRM:
        {
            build_fixed_time_render(used_scope, signal_render_data_out);
            break;
        }

        case SCOPE_MODE_SCROLL_TO_RIGHT_SRM:
        {
            build_scroll_render(used_scope, signal_render_data_out);
            break;
        }

        case SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM:
        {
            build_period_render(used_scope, signal_render_data_out);
            break;
        }

        default: break;
    }

    */
}


void build_fixed_time_render(Scope* used_scope, signal_render_ctx* render_data)
{

}


void build_scroll_render(Scope* used_scope, signal_render_ctx* render_data)
{

}

void build_fixed_period_render(Scope* used_scope, signal_render_ctx* render_data)
{

}


void build_render(Scope* used_scope, signal_render_ctx* render_data)
{

}


void scope_signal_info_gui_renew(Scope* used_scope)
{

}


void draw_signal(Scope* used_scope, SDL_Renderer* renderer)
{
    if (!used_scope) return;

    scope_render_ctx* r = &used_scope->scope_render_data;
    signal_render_ctx* signal = &r->signal_render_data;

    // Цвет линии сигнала (можешь сделать отдельный main_color)
    SDL_Color color = r->main_color_5;

    SDL_SetRenderDrawColor(

        renderer,
        color.r,
        color.g,
        color.b,
        color.a
        
    );

    // Рисуем точки
    for (int i = 0; i < signal->size; i++)
    {
        signal_render_point* p = &signal->points[i];

        if (!p->show) continue;

        int signal_width = used_scope->scope_render_data.basic_border_thickness_2;

        for (int i = - signal_width / 2; i == signal_width / 2; i++)
        {
            my_sdl_draw_pixel(renderer, p->x, p->y + i, color);
        }
    }
}


// Изменение цвета дисплея - дребезжание
void scope_display_animation(Scope* used_scope)
{
    SDL_Color noised_color;

    if (used_scope->main_settings.current_state == ON_SS)
    {
        SDL_Color base_color = used_scope->scope_render_data.main_color_3;

        int noise_amplitude = 4;

        noised_color.r =
            SDL_clamp(
                base_color.r +
                (rand() % (noise_amplitude * 2 + 1) - noise_amplitude),
                0, 255
            );

        noised_color.g =
            SDL_clamp(
                base_color.g +
                (rand() % (noise_amplitude * 2 + 1) - noise_amplitude),
                0, 255
            );

        noised_color.b =
            SDL_clamp(
                base_color.b +
                (rand() % (noise_amplitude * 2 + 1) - noise_amplitude),
                0, 255
            );

        noised_color.a = base_color.a;
    }

    else 
    {
        noised_color = hex_to_sdl_color("#1b1913", 255);

    }

    used_scope->scope_render_data.gui_parameters.display_fill_color = noised_color;


}


// =========================================================================================== Scope GUI


// =========================================================================================== Scope buttons callbacks


void decrease_scope_value_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}

void increase_scope_value_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}


void decrease_scope_time_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}

void increase_scope_time_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}



void decrease_amplitude(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}

void increase_amplitude(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}


void decrease_frequency(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}

void increase_frequency(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}


void change_controlled_signal(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}


void change_scope_render_mode(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}


void play_controlled_signal(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


}


void on_off_command(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;

    int curr = used_scope->main_settings.current_state;

    if (curr == 0)
        used_scope->main_settings.current_state = 1;
    else
        used_scope->main_settings.current_state = 0;


    if (used_scope->main_settings.current_state == ON_SS)
    {
        used_scope->scope_render_data.scope_on_off_button.pressed_color = used_scope->scope_render_data.main_color_4;
    }
    else 
    {
        scope_signal_buffer_clear(used_scope);
        scope_running_signal_characteristics_clear(used_scope);
        scope_measured_signal_characteristics_clear(used_scope);
        scope_filter_clear(used_scope);
        scope_peaks_ctx_clear(used_scope);
        scope_wave_pattern_detector_former_clear(used_scope);
        scope_wave_pattern_detector_clear(used_scope);
        
        used_scope->scope_render_data.scope_on_off_button.pressed_color = used_scope->scope_render_data.main_color_5;


        used_scope->signal_control_data.type_of_controlled_signal = CLEAN_CST;
    }
}

// =========================================================================================== Scope buttons callbacks


// =========================================================================================== API realization


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

    scope_gui_init(used_scope, renderer);

    scope_gui_renew(used_scope);
    scope_screen_gui_init(used_scope);

    // ===== Инициализация GUI ===== 
}


void signal_check(Scope* used_scope, sin_generator_ctx* controlled_signal)
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


void scope_render(Scope* used_scope)
{
    // TEST: Background 0
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.background_x_2,
        used_scope->scope_render_data.gui_parameters.background_y_2,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        hex_to_sdl_color("#b8f87c", 254),
        used_scope->scope_render_data.gui_parameters.background_border_thickness_1,
        hex_to_sdl_color("#b8f87c", 254)

    );


    // Background 1
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.background_x_1,
        used_scope->scope_render_data.gui_parameters.background_y_1,
        used_scope->scope_render_data.gui_parameters.background_w_1,
        used_scope->scope_render_data.gui_parameters.background_h_1,
        used_scope->scope_render_data.gui_parameters.background_fill_color_1,
        used_scope->scope_render_data.gui_parameters.background_border_thickness_1,
        used_scope->scope_render_data.gui_parameters.background_border_color_1

    );

    // Scope name
    Textbox_render(&used_scope->scope_render_data.scope_signature_textbox, used_scope->scope_render_data.renderer);


    // Background 2
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.background_x_2,
        used_scope->scope_render_data.gui_parameters.background_y_2,
        used_scope->scope_render_data.gui_parameters.background_w_2,
        used_scope->scope_render_data.gui_parameters.background_h_2,
        used_scope->scope_render_data.gui_parameters.background_fill_color_2,
        used_scope->scope_render_data.gui_parameters.background_border_thickness_2,
        used_scope->scope_render_data.gui_parameters.background_border_color_2

    );


    // Background 3
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.background_x_3,
        used_scope->scope_render_data.gui_parameters.background_y_3,
        used_scope->scope_render_data.gui_parameters.background_w_3,
        used_scope->scope_render_data.gui_parameters.background_h_3,
        used_scope->scope_render_data.gui_parameters.background_fill_color_3,
        used_scope->scope_render_data.gui_parameters.background_border_thickness_2,
        used_scope->scope_render_data.gui_parameters.background_border_color_3

    );


    // Display
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.display_x,
        used_scope->scope_render_data.gui_parameters.display_y,
        used_scope->scope_render_data.gui_parameters.display_w,
        used_scope->scope_render_data.gui_parameters.display_h,
        used_scope->scope_render_data.gui_parameters.display_fill_color,
        used_scope->scope_render_data.gui_parameters.display_border_thickness,
        used_scope->scope_render_data.gui_parameters.display_border_color

    );

    // Second display
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.scope_info_zone_display_x_1,
        used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1,
        used_scope->scope_render_data.gui_parameters.scope_info_zone_display_w_1,
        used_scope->scope_render_data.gui_parameters.scope_info_zone_display_h_1,
        used_scope->scope_render_data.gui_parameters.display_fill_color,
        used_scope->scope_render_data.gui_parameters.display_border_thickness,
        used_scope->scope_render_data.gui_parameters.display_border_color

    );


    // Mesh + текстбоксы + свечение при включенных дисплеях
    if (used_scope->main_settings.current_state == ON_SS)
    {

        SDL_Color base_color =
        used_scope->scope_render_data.gui_parameters.display_fill_color;

        const int layers = 25;
        const int light_thickness = 100;

        for (int i = 0; i < layers; i++)
        {
            SDL_Color glow_color = base_color;
        
            // Нелинейное затухание

            Uint8 new_glow = layers - i;

            if (new_glow > 0) glow_color.a = new_glow;
            else glow_color.a = 1;

            // Свет дисплей 1
            my_sdl_draw_rect(
        
                used_scope->scope_render_data.renderer,
        
                used_scope->scope_render_data.gui_parameters.display_x,
                used_scope->scope_render_data.gui_parameters.display_y,
        
                used_scope->scope_render_data.gui_parameters.display_w + (i + 1) * light_thickness / layers - 2 * used_scope->scope_render_data.gui_parameters.display_border_thickness,
                used_scope->scope_render_data.gui_parameters.display_h + (i + 1) * light_thickness / layers - 2 * used_scope->scope_render_data.gui_parameters.display_border_thickness,
        
                light_thickness / layers,
        
                glow_color
            );


            // Такой же свет дисплей 2
            my_sdl_draw_rect(

                used_scope->scope_render_data.renderer,
        
                used_scope->scope_render_data.gui_parameters.scope_info_zone_display_x_1,
                used_scope->scope_render_data.gui_parameters.scope_info_zone_display_y_1,
        
                used_scope->scope_render_data.gui_parameters.scope_info_zone_display_w_1 + (i + 1) * light_thickness / layers - 2 * used_scope->scope_render_data.gui_parameters.display_border_thickness,
                used_scope->scope_render_data.gui_parameters.scope_info_zone_display_h_1 + (i + 1) * light_thickness / layers - 2 * used_scope->scope_render_data.gui_parameters.display_border_thickness,
        
                light_thickness / layers,
        
                glow_color
            );
        }

        // Горизонтальные линии 2 - 8

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_2_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_2_y1,
            used_scope->scope_render_data.gui_parameters.h_line_2_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_2_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_2_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_3_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_3_y1,
            used_scope->scope_render_data.gui_parameters.h_line_3_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_3_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_3_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_4_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_4_y1,
            used_scope->scope_render_data.gui_parameters.h_line_4_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_4_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_4_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_5_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_5_y1,
            used_scope->scope_render_data.gui_parameters.h_line_5_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_5_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_5_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_6_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_6_y1,
            used_scope->scope_render_data.gui_parameters.h_line_6_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_6_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_6_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_7_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_7_y1,
            used_scope->scope_render_data.gui_parameters.h_line_7_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_7_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_7_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.h_line_8_x1, 
            used_scope->scope_render_data.gui_parameters.h_line_8_y1,
            used_scope->scope_render_data.gui_parameters.h_line_8_x2, 
            used_scope->scope_render_data.gui_parameters.h_line_8_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.h_line_8_color
                            
        );


        // Вертикальные линии 2 - 16

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_2_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_2_y1,
            used_scope->scope_render_data.gui_parameters.v_line_2_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_2_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_2_color
                            
        );


        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_3_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_3_y1,
            used_scope->scope_render_data.gui_parameters.v_line_3_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_3_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_3_color
                            
        );


        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_4_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_4_y1,
            used_scope->scope_render_data.gui_parameters.v_line_4_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_4_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_4_color
                            
        );


        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_5_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_5_y1,
            used_scope->scope_render_data.gui_parameters.v_line_5_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_5_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_5_color
                            
        );


        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_6_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_6_y1,
            used_scope->scope_render_data.gui_parameters.v_line_6_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_6_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_6_color
                            
        );


        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_7_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_7_y1,
            used_scope->scope_render_data.gui_parameters.v_line_7_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_7_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_7_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_8_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_8_y1,
            used_scope->scope_render_data.gui_parameters.v_line_8_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_8_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_8_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_9_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_9_y1,
            used_scope->scope_render_data.gui_parameters.v_line_9_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_9_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_9_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_10_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_10_y1,
            used_scope->scope_render_data.gui_parameters.v_line_10_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_10_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_10_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_11_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_11_y1,
            used_scope->scope_render_data.gui_parameters.v_line_11_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_11_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_11_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_12_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_12_y1,
            used_scope->scope_render_data.gui_parameters.v_line_12_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_12_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_12_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_13_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_13_y1,
            used_scope->scope_render_data.gui_parameters.v_line_13_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_13_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_13_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_14_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_14_y1,
            used_scope->scope_render_data.gui_parameters.v_line_14_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_14_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_14_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_15_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_15_y1,
            used_scope->scope_render_data.gui_parameters.v_line_15_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_15_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_15_color
                            
        );

        my_sdl_draw_line(
            
            used_scope->scope_render_data.renderer,
            used_scope->scope_render_data.gui_parameters.v_line_16_x1, 
            used_scope->scope_render_data.gui_parameters.v_line_16_y1,
            used_scope->scope_render_data.gui_parameters.v_line_16_x2, 
            used_scope->scope_render_data.gui_parameters.v_line_16_y2,
            used_scope->scope_render_data.gui_parameters.lines_thickness,
            used_scope->scope_render_data.gui_parameters.v_line_16_color
                            
        );

        /*
            // Обновляем графику под рендер (даже для первых точек)
            scope_screens_gui_renew_by_signal_data(used_scope);

            // Заполняемся точками
            signal_render_ctx* signal;

            signal = &used_scope->scope_render_data.signal_render_data;

            build_fixed_time_render(used_scope, signal);

            // Рисуем сигнал
            draw_signal(used_scope, used_scope->scope_render_data.renderer);
        */

        // Текущие текстбоксы информации о сигнале
        Textbox_render(&used_scope->scope_render_data.signal_scale_textbox, used_scope->scope_render_data.renderer);

        Textbox_render(&used_scope->scope_render_data.time_scale_textbox, used_scope->scope_render_data.renderer);

        Textbox_render(&used_scope->scope_render_data.frequency_or_period_textbox, used_scope->scope_render_data.renderer);

        Textbox_render(&used_scope->scope_render_data.amplitude_textbox, used_scope->scope_render_data.renderer);

    }
    
    // Кнопки и пояснения

    // Изменение масштаба значения сигнала

    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_x_1,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_w_1,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_h_1,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_fill_color_1,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_border_thickness_1,
        used_scope->scope_render_data.gui_parameters.value_scale_set_info_border_color_1

    );

    Textbox_render(&used_scope->scope_render_data.change_value_scale_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.decrease_value_scale_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.increase_value_scale_button, used_scope->scope_render_data.renderer);
    


    // Изменение масштаба времени сигнала

    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_x_1,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_y_1,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_w_1,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_h_1,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_fill_color_1,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_border_thickness_1,
        used_scope->scope_render_data.gui_parameters.time_scale_set_info_border_color_1

    );

    Textbox_render(&used_scope->scope_render_data.change_time_scale_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.decrease_time_scale_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.increase_time_scale_button, used_scope->scope_render_data.renderer);


    // Изменение амплитуды сигнала

    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_x_1,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_y_1,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_w_1,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_h_1,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_fill_color_1,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_border_thickness_1,
        used_scope->scope_render_data.gui_parameters.amplitude_set_info_border_color_1

    );

    Textbox_render(&used_scope->scope_render_data.change_amplitude_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.decrease_amplitude_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.increase_amplitude_button, used_scope->scope_render_data.renderer);


    // Изменение частоты сигнала

    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_x_1,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_y_1,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_w_1,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_h_1,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_fill_color_1,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_border_thickness_1,
        used_scope->scope_render_data.gui_parameters.frequency_set_info_border_color_1

    );

    Textbox_render(&used_scope->scope_render_data.change_frequency_instruction_textbox, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.decrease_frequency_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.increase_frequency_button, used_scope->scope_render_data.renderer);


    Button_render(&used_scope->scope_render_data.signal_change_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.mode_change_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.controlled_signal_play_button, used_scope->scope_render_data.renderer);

    Button_render(&used_scope->scope_render_data.scope_on_off_button, used_scope->scope_render_data.renderer);

}


void scope_destroy(Scope* used_scope)
{
    if (!used_scope) return;

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


    // ===== 3. буфер =====
    // НЕ НУЖНО free — он static array внутри struct
    // но можно “обнулить состояние”

    used_scope->signal_control_data.scope_buffer_data.head = 0;
    used_scope->signal_control_data.scope_buffer_data.count = 0;


    used_scope = NULL;
}


// =========================================================================================== API realization