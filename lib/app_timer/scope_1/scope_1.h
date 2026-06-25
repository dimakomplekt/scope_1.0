// scope.h

#pragma once

// =========================================================================================== IMPORT

#include "../engine.h"

#include "../sin_generator/sin_generator.h"

#include "../SDL2/UI_elements/my_sdl_button/my_sdl_button.h"
#include "../SDL2/UI_elements/my_sdl_textbox/my_sdl_textbox.h"

#include <stdbool.h>

// =========================================================================================== IMPORT



// =========================================================================================== SCOPE MAIN SETTINGS

// RAW LVL
#define SCOPE_SAMPLE_RATE                   48000
#define MIN_CONTROLLED_PERIOD               2
#define SCOPE_BUFFER_STOCK                  4

// VIEW LVL
#define MAX_DISPLAY_WIDTH                   2000
#define SCOPE_SCREEN_OVERSAMPLING           4

#define MAX_ZERO_CROSSINGS_TO_CHECK         64

#define BUFFER_SIZE                         (SCOPE_SAMPLE_RATE * MIN_CONTROLLED_PERIOD * SCOPE_BUFFER_STOCK)
#define RENDER_POINTS_BUFFER_SIZE           (MAX_DISPLAY_WIDTH * SCOPE_SCREEN_OVERSAMPLING)

#define FREQ_TO_SEPARATE_MODES_LB           230
#define FREQ_TO_SEPARATE_MODES_HB           250

#define ZC_THRESHOLD_START_VALUE 0.005f


// ===== Scope mode enum =====

// Включен или выключен
typedef enum scope_state
{
    
    OFF_SS,
    ON_SS,

    LIMIT_SS

} scope_state;


// Режим рендеринга
typedef enum scope_render_mode
{

    SCOPE_MODE_FIXED_TIME_STEP_SRM,
    SCOPE_MODE_SCROLL_TO_RIGHT_SRM,
    SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM,

    LIMIT_SRM

} scope_render_mode;


// Текущие единицы отображения сигнала
typedef enum signal_units
{

    VOLTS_SU,
    
    LIMIT_SU

} signal_units;


// Текущие единицы отображения времени или периода
typedef enum time_units
{

    NANOSECONDS_TU,
    MICROSECONDS_TU,
    MILLISECONDS_TU,
    SECONDS_TU,

    LIMIT_TU
    
} time_units;


// Текущие единицы отображения частоты
typedef enum frequncy_units
{

    MILLIHERTZ_FU,
    HERTZ_FU,
    KILOHERTZ_FU,
    MEGAHERTZ_FU,

    LIMIT_FU
    
} frequncy_units;


// Тип контролируемого сигнала - КОСТЫЛЬ ПОД ТЕКУЩУЮ ЗАДАЧУ
typedef enum controlled_signal_type
{

    CLEAN_CST,
    NOISED_CST,
    
    LIMIT_CST
    
} controlled_signal_type;



// =========================================================================================== SCOPE MAIN SETTINGS


// =========================================================================================== SCOPE MAIN BUFFER

// Кольцевой буфер - пишем всегда в head, при переполнении начинаем перезаписывать старую дату, чтение
// производим для элементов, которые стоят до head (в случае переполнения при переходе head в [0] и 
// чтении [head - 1] получим чтение head[BUFFER_SIZE]) соотв. дата всегда будет адекватной без необходимости
// производить сдвиги значений


// Одиночный сэмпл основного буффера
typedef struct sample {

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


// =========================================================================================== SCOPE MAIN BUFFER


// =========================================================================================== SCOPE REALTIME ANALYSER


// ===== Основные характеристики всего сигнала =====

// Среднее значение
typedef struct running_mean_ctx {

    float mean;

    float alpha;        // EMA alpha

} running_mean_ctx;

// Медиана
typedef struct running_median_ctx {

    float median;

    float step;        // скорость адаптации
    float drift;       // накопленный перекос

} running_median_ctx;


// Общий контекст фильтрации инпута
typedef struct scope_realtime_filtering_ctx {

    float running_betha;
    float running_sigma;

    // Коэффициент трешхолда
    float k_treshold;
    float running_treshold;

} scope_realtime_filtering_ctx;


// Общий контекст running основных характеристик
typedef struct scope_running_signal_data_ctx {

    running_mean_ctx running_mean;
    running_median_ctx running_median;

    // Как 0.7 * median + 0.3 * mean;
    // С изменением долей по результатам прохода measured
    float mean_part_in_offset;
    float median_part_in_offset;

    float running_dc_offset;

} scope_running_signal_data_ctx;


// Общий контекст measured-основных характеристик
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

// ===== Основные характеристики всего сигнала =====


// ===== Полуволна =====

// Типы трендов
typedef enum trend_type {

    RISING_PT,      // Восходящий сигнал
    FALLING_PT,     // Нисходящий сигнал

    LIMIT_PT

} trend_type;

// Контекст анализатора переходов, который хранит данные
// о переходах и выводит их обработку в буффер zero_crosses_detector с
// определенным количеством точек cleaned_zero_cross, к примеру - 128 точками
typedef enum wave_pattern_buffer_former_states
{
    WAIT_RISING_MINUS_FS,
    WAIT_RISING_PLUS_FS,

    WAIT_FALLING_PLUS_FS,
    WAIT_FALLING_MINUS_FS,

    LIMIT_FS

} wave_pattern_buffer_former_states;


// Общий контекст пиков
// используется в peak_detector для поиска runtime пиков 
// и характеристик контролируемых полуволн
typedef struct scope_realtime_peaks_ctx {
    
    trend_type prev_trend;              // Предыдущий тренд сигнала

    float trend_confidence;
    float last_event_confidence;

    float peak_candidate;               // Кандидат на пик
    float trough_candidate;             // Кандидат на яму

    float last_peak;                    // Прошлый пик
    float last_trough;                  // Прошлая яма

    float max_candidate;                // Кандидат на максимум
    float min_candidate;                // Кандидат на минимум

    float max_confidence;               // Уверенность в кандидате
    float min_confidence;               // Уверенность в кандидате

    float running_min;                  // Текущее минимальное значение
    float running_max;                  // Текущее максимальное значение

    float running_amplitude;            // Текущая амплитуда


    // Инкрементальный счётчик количества измерений сигнала в текущей полуволне
    int halfwave_parts_counter;

    // Средняя скорость текущей полуволны
    float average_halfwave_velocity;

    // Площадь текущей полуволны
    float halfwave_area;

} scope_realtime_peaks_ctx;


// Очищенное время перехода, вычисленное на базе суммы t деленной на 2,
// от переходов без смены курса, через dc_offset - treshold и dc_offset + treshold
// Оформляется для текущей контролируемой полуволны в peak_detector 
typedef struct cleaned_zero_cross
{

    trend_type zero_cross_type;         // RISING (из - в +) или FALLING (из + в -)

    double time;                        // Время прохода

} cleaned_zero_cross;


// Данные о полуволне, по которым возможно провести анализ паттерна
// Оформляется для текущей контролируемой полуволны в peak_detector 
typedef struct halfwave_zero_cross_ctx
{
    // За 1 проход на приёме данных от buffer_former

    trend_type halfwave_type;            // Характер полуволны 

    double start_time;
    double end_time;

    // По принятым данным 
    double halfwave_full_time;           // Примерное полное время полуволны
    

    // За тот же проход на приёме данных по главному буфферу :

    float peak_value;                    // Максимум (по главному буфферу от head - до head.halfwave_full_time)
    float trough_value;                  // Минимум (по главному буфферу от head - до head.halfwave_full_time)
    
    float halfwave_area;                 // Примерная площадь этой полуволны (по главному буфферу от head - до head.halfwave_full_time)
    float halfwave_average_speed;        // Примерная средняя скорость изменения значений в этой полуволне (по главному буфферу от head - до head.halfwave_full_time)

} halfwave_zero_cross_ctx;


// Двухпороговый захват времени с переобновлением текущей границы при повторных входах
typedef struct wave_pattern_detector_former_ctx
{

    /*

        я зашел в 1 зону, у меня rised_without_fall стоит false, сколько раз я бы не дропался и не возвращалолся,
        пока он false я просто обновляю rising_dc_minus_th_time на новое при новом заходе, как только я пересекаю 
        dc_offset + treshold, я ставлю rised_without_fall, как true, ставлю falled_without_rise, как false
        и curr_waited_type переключаю на FALLING

    */

    wave_pattern_buffer_former_states buffer_former_state;    // Переход какой точки RISING или FALLING через 0 ожидается на приход в формер

    halfwave_zero_cross_ctx curr_halfwave;                    // Текущая анализируемая полуволна, которая при окончании анализа будет передана в zero_crosses_detector


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


} wave_pattern_detector_former_ctx;


// Хранит данные о последних 64 (128 / 2) вычищенных от шума полуволны с их показателями
// Используется в анализаторе паттернов и непосредственно при расчёте периода 
typedef struct wave_pattern_detector_ctx 
{
    halfwave_zero_cross_ctx halfwaves_for_detection[64];

    int head;
    int count;

} wave_pattern_detector_ctx;

// ===== Полуволна =====

// =========================================================================================== SCOPE REALTIME ANALYSER


// =========================================================================================== SCOPE PERIOD AND PEAKS ANALYSER


typedef struct pattern_candidate
{

    int L;
    int offset;
    float error;
    float repeat_score;

} pattern_candidate;


typedef struct pattern_result
{

    int period_len;     // длина периода (в полуволнах)
    int offset;         // фазовый сдвиг в буфере
    float confidence;   // уверенность совпадения

} pattern_result;


// =========================================================================================== SCOPE PERIOD AND PEAKS ANALYSER


// =========================================================================================== SCOPE RENDERING


// ===== Scope display render data =====

// RAW → ANALYSIS → VIEW (DISPLAY) → PIXELS
typedef struct signal_render_point
{

    int x;
    int y;
    bool show;

} signal_render_point;


typedef struct signal_render_ctx
{

    signal_render_point points[RENDER_POINTS_BUFFER_SIZE];        // динамический массив
    int size;                                                     // сколько точек (ширина дисплея)

} signal_render_ctx;


// ===== Scope display render data =====


// ===== Scope render data =====

// Структура данных основных фигур для рендера

typedef struct scope_gui_basic_parameters
{
    /*

        Данные для вызова функций рендера фигур (в данной версии - задник, дисплей, сетка)


        void my_sdl_draw_line(

            SDL_Renderer* renderer,
            int x1, int y1,
            int x2, int y2,
            int thickness,
            SDL_Color color

        )


        void my_sdl_draw_filled_rect_bi(

            SDL_Renderer* renderer,
            int x, int y,
            int w, int h,
            SDL_Color fill_color,
            int border_thickness,
            SDL_Color border_color,

        );

    */

    /*
        Осциллограф состоит из нескольких фигур

        Общий размер - 22 на 14 единиц (по 50 пикселей в единице)

        - Задник (3 части - верхняя, средняя, нижняя)
            - Верхняя часть - 22 на 1 единицу
            - Средняя часть - 22 на 12 единиц
            - Нижняя часть - 22 на 1 единицу

        - Дисплей - 16 на 8 единиц (находится с отступами в 1 единицу от левого и верхнего края задника)
        - Сетка (8 горизонтальных линий + 16 вертикальных линий)
            - Все линии сетки - 16 на 8 единиц (находятся внутри дисплея без отступов)

        - Кнопки и поясняющие блоки
            - Блок пояснений (прямоугольник + текстбокс) + кнопка + кнопка смены режима для масштаба сигнала - 3 на 1 единицу (находится с отступом в 1 единицу от правого края задника с выравниванием под верхний край дисплея)
            - Блок пояснений (прямоугольник + текстбокс) + кнопка + кнопка смены режима для масштаба времени - 3 на 1 единицу (находится с отступом в 1 единицу от правого края задника и с выравниванием под нижний край блока смены масштаба сигнала)
            - Блок пояснений(прямоугольник + текстбокс) + кнопка + кнопка смены режима для частоты сигнала - 3 на 1 единицу (находится с отступом в 1 единицу от правого края задника и с выравниванием под нижний край блока смены масштаба времени)
            - Кнопка для смены режима отображения - 3 на 1 единицу (находится с отступом в 1 единицу от правого края задника и по центру по вертикали с отступом в 1 единицу от низа блока для частоты сигнала)
            - Кнопка для смены сигнала - 3 на 1 единицу (находится с отступом в 1 единицу от правого края задника и по центру по вертикали с отступом в 1 единицу от низа кнопки для смены режима отображения)
   
            - Поясняющий блок (прямоугольник + текстбокс) для масштаба сигнала - 2 на 1 единицу (x - по уровню вертикальной линии сетки номер 3, верхний край - с отступом в 1 единицу от нижнего края экрана)
            - Поясняющий блок (прямоугольник + текстбокс) для масштаба времени (или кол-ва периодов в режиме 2) - 2 на 1 единицу (x - по уровню вертикальной линии сетки номер 7, верхний край - с отступом в 1 единицу от нижнего края экрана)
            - Вычисленная амплитуда сигнала (прямоугольник + текстбокс) - 2 на 1 единицу (x - по уровню вертикальной линии сетки номер 11, верхний край - с отступом в 1 единицу от нижнего края экрана)
            - Вычисленная частота сигнала (или период в режиме 2) (прямоугольник + текстбокс) - 2 на 1 единицу (x - по уровню вертикальной линии сетки номер 15, верхний край - с отступом в 1 единицу от нижнего края экрана)

            UPD: в итоге добавилась пара других кнопок и элементов, но суть похожая, переписывать коммент-пояснялку лень
        */


    // Базовые настройки отображения


    int width_units;
    int height_units;

    int bg_1_width_units;
    int bg_1_height_units;

    int bg_2_width_units;
    int bg_2_height_units;

    int bg_3_width_units;
    int bg_3_height_units;


    int display_width_units;
    int display_height_units;

    int buttons_signature_width_units;
    int buttons_signature_height_units;

    int buttons_1_width_units;
    int buttons_1_height_units;

    int buttons_2_width_units;
    int buttons_2_height_units;

    int info_panels_width_units;
    int info_panels_height_units;

    int margin_units;
    

    // ===== Задник =====

    // Верхняя часть

    int background_x_1;
    int background_y_1;

    int background_w_1;
    int background_h_1;

    SDL_Color background_fill_color_1;

    SDL_Color background_border_color_1;

    int background_border_thickness_1;


    // Средняя часть

    int background_x_2;
    int background_y_2;

    int background_w_2;
    int background_h_2;

    SDL_Color background_fill_color_2;

    SDL_Color background_border_color_2;

    int background_border_thickness_2;


    // Нижняя часть
    
    int background_x_3;
    int background_y_3;

    int background_w_3;
    int background_h_3;

    SDL_Color background_fill_color_3;

    SDL_Color background_border_color_3;

    int background_border_thickness_3;


    // ===== Дисплей =====

    int display_x;
    int display_y;

    int display_w;
    int display_h;

    int points_to_render_signal;    // Через ширину дисплея и оверсемплинг 


    SDL_Color display_fill_color;

    SDL_Color display_border_color;

    int display_border_thickness;


    // ===== Сетка =====

    // 8 горизонтальных линий + 16 вертикальных линий для сетки

    int lines_thickness;

    // H Line 1

    int h_line_1_x1;
    int h_line_1_y1;

    int h_line_1_x2;
    int h_line_1_y2;

    SDL_Color h_line_1_color;


    // H Line 2

    int h_line_2_x1;
    int h_line_2_y1;
    
    int h_line_2_x2;
    int h_line_2_y2;

    SDL_Color h_line_2_color;


    // H Line 3

    int h_line_3_x1;
    int h_line_3_y1;
    
    int h_line_3_x2;
    int h_line_3_y2;

    SDL_Color h_line_3_color;


    // H Line 4

    int h_line_4_x1;
    int h_line_4_y1;
    
    int h_line_4_x2;
    int h_line_4_y2;

    SDL_Color h_line_4_color;


    // H Line 5

    int h_line_5_x1;
    int h_line_5_y1;
    
    int h_line_5_x2;
    int h_line_5_y2;

    SDL_Color h_line_5_color;


    // H Line 6

    int h_line_6_x1;
    int h_line_6_y1;
    
    int h_line_6_x2;
    int h_line_6_y2;

    SDL_Color h_line_6_color;


    // H Line 7

    int h_line_7_x1;
    int h_line_7_y1;
    
    int h_line_7_x2;
    int h_line_7_y2;

    SDL_Color h_line_7_color;


    // H Line 8

    int h_line_8_x1;
    int h_line_8_y1;
    
    int h_line_8_x2;
    int h_line_8_y2;

    SDL_Color h_line_8_color;


    // H Line 9

    int h_line_9_x1;
    int h_line_9_y1;
    
    int h_line_9_x2;
    int h_line_9_y2;

    SDL_Color h_line_9_color;


    // V Line 1

    int v_line_1_x1;
    int v_line_1_y1;

    int v_line_1_x2;
    int v_line_1_y2;

    SDL_Color v_line_1_color;


    // V Line 2

    int v_line_2_x1;
    int v_line_2_y1;
    
    int v_line_2_x2;
    int v_line_2_y2;

    SDL_Color v_line_2_color;


    // V Line 3

    int v_line_3_x1;
    int v_line_3_y1;
    
    int v_line_3_x2;
    int v_line_3_y2;

    SDL_Color v_line_3_color;


    // V Line 4

    int v_line_4_x1;
    int v_line_4_y1;
    
    int v_line_4_x2;
    int v_line_4_y2;

    SDL_Color v_line_4_color;


    // V Line 5

    int v_line_5_x1;
    int v_line_5_y1;
    
    int v_line_5_x2;
    int v_line_5_y2;

    SDL_Color v_line_5_color;


    // V Line 6

    int v_line_6_x1;
    int v_line_6_y1;
    
    int v_line_6_x2;
    int v_line_6_y2;

    SDL_Color v_line_6_color;


    // V Line 7

    int v_line_7_x1;
    int v_line_7_y1;
    
    int v_line_7_x2;
    int v_line_7_y2;

    SDL_Color v_line_7_color;


    // V Line 8

    int v_line_8_x1;
    int v_line_8_y1;
    
    int v_line_8_x2;
    int v_line_8_y2;

    SDL_Color v_line_8_color;


    // V Line 9

    int v_line_9_x1;
    int v_line_9_y1;
    
    int v_line_9_x2;
    int v_line_9_y2;

    SDL_Color v_line_9_color;


    // V Line 10

    int v_line_10_x1;
    int v_line_10_y1;
    
    int v_line_10_x2;
    int v_line_10_y2;

    SDL_Color v_line_10_color;


    // V Line 11

    int v_line_11_x1;
    int v_line_11_y1;
    
    int v_line_11_x2;
    int v_line_11_y2;

    SDL_Color v_line_11_color;


    // V Line 12

    int v_line_12_x1;
    int v_line_12_y1;
    
    int v_line_12_x2;
    int v_line_12_y2;

    SDL_Color v_line_12_color;


    // V Line 13

    int v_line_13_x1;
    int v_line_13_y1;
    
    int v_line_13_x2;
    int v_line_13_y2;

    SDL_Color v_line_13_color;


    // V Line 14

    int v_line_14_x1;
    int v_line_14_y1;
    
    int v_line_14_x2;
    int v_line_14_y2;

    SDL_Color v_line_14_color;


    // V Line 15

    int v_line_15_x1;
    int v_line_15_y1;
    
    int v_line_15_x2;
    int v_line_15_y2;

    SDL_Color v_line_15_color;


    // V Line 16

    int v_line_16_x1;
    int v_line_16_y1;
    
    int v_line_16_x2;
    int v_line_16_y2;

    SDL_Color v_line_16_color;


    // V Line 17

    int v_line_17_x1;
    int v_line_17_y1;
    
    int v_line_17_x2;
    int v_line_17_y2;

    SDL_Color v_line_17_color;


    // ===== Кнопки и пояснения =====
    
    SDL_Color description_text_color;   // Общий цвет для всех пояснений


    // Прямоугольник пояснения и координаты для текстбокса инструкции по изменению масштаба сигнала

    int value_scale_set_info_x_1;
    int value_scale_set_info_y_1;

    int value_scale_set_info_w_1;
    int value_scale_set_info_h_1;

    SDL_Color value_scale_set_info_fill_color_1;

    SDL_Color value_scale_set_info_border_color_1;

    int value_scale_set_info_border_thickness_1;

    // Координаты кнопки уменьшения масштаба сигнала
    
    int signal_scale_decrease_button_x_1;
    int signal_scale_decrease_button_y_1;
    
    // Координаты кнопки увеличения масштаба сигнала

    int signal_scale_increase_button_x_1;
    int signal_scale_increase_button_y_1;


    // Прямоугольник пояснения и координаты для текстбокса инструкции по изменению масштаба времени

    int time_scale_set_info_x_1;
    int time_scale_set_info_y_1;

    int time_scale_set_info_w_1;
    int time_scale_set_info_h_1;

    SDL_Color time_scale_set_info_fill_color_1;

    SDL_Color time_scale_set_info_border_color_1;

    int time_scale_set_info_border_thickness_1;


    // Координаты кнопки уменьшения масштаба времени

    int time_scale_decrease_button_x_1;
    int time_scale_decrease_button_y_1;
    
    // Координаты кнопки увеличения масштаба времени

    int time_scale_increase_button_x_1;
    int time_scale_increase_button_y_1;


    // Прямоугольник пояснения и координаты для текстбокса инструкции по изменению амплитуды сигнала

    int amplitude_set_info_x_1;
    int amplitude_set_info_y_1;

    int amplitude_set_info_w_1;
    int amplitude_set_info_h_1;

    SDL_Color amplitude_set_info_fill_color_1;

    SDL_Color amplitude_set_info_border_color_1;

    int amplitude_set_info_border_thickness_1;


    // Координаты кнопки уменьшения сигнала
    int amplitude_decrease_button_x_1;
    int amplitude_decrease_button_y_1;
    
    // Координаты кнопки увеличения сигнала

    int amplitude_increase_button_x_1;
    int amplitude_increase_button_y_1;


    // Прямоугольник пояснения и координаты для текстбокса инструкции по изменению частоты сигнала

    int frequency_set_info_x_1;
    int frequency_set_info_y_1;

    int frequency_set_info_w_1;
    int frequency_set_info_h_1;

    SDL_Color frequency_set_info_fill_color_1;

    SDL_Color frequency_set_info_border_color_1;

    int frequency_set_info_border_thickness_1;


    // Координаты кнопки уменьшения частоты 
    int frequency_decrease_button_x_1;
    int frequency_decrease_button_y_1;
    
    // Координаты кнопки увеличения частоты 

    int frequency_increase_button_x_1;
    int frequency_increase_button_y_1;


    // Координаты кнопки смены сигнала

    int signal_change_button_x_1;
    int signal_change_button_y_1;

    // Координаты кнопки смены режима отображения
    int image_regime_change_button_x_1;
    int image_regime_change_button_y_1;
    

    // Координаты кнопки воспроизведения сигнала
    int controlled_signal_play_button_x_1;
    int controlled_signal_play_button_y_1;


    // Координаты кнопки включения / выключения
    int scope_on_off_button_x_1;
    int scope_on_off_button_y_1;


    // ===== Информационный дисплей =====

    // Координаты и габариты
    int scope_info_zone_display_x_1;
    int scope_info_zone_display_y_1;

    int scope_info_zone_display_w_1;
    int scope_info_zone_display_h_1;

    // Цвета наследуются от основного экрана


    // Текстбокс для показа масштаба сигнала

    int signal_scale_info_x_1;
    int signal_scale_info_y_1;

    SDL_Color signal_scale_info_fill_color_1;

    // Текстбокс для масштаба времени

    int time_scale_info_x_1;
    int time_scale_info_y_1;

    SDL_Color time_scale_info_fill_color_1;

    // Текстбокс для амплитуды сигнала

    int amplitude_info_x_1;
    int amplitude_info_y_1;

    SDL_Color amplitude_info_fill_color_1;

    // Текстбокс для частоты сигнала

    int frequency_or_period_info_x_1;
    int frequency_or_period_info_y_1;

    SDL_Color frequency_or_period_info_fill_color_1;

} scope_gui_basic_parameters;



typedef struct scope_render {

    // Ссылка на рендерер
    SDL_Renderer* renderer;

    // Глобальные настройки

    // Позиция элемента
    int x_position;
    int y_position;

    // Цвета

    SDL_Color main_color_1;         // Background 1                     = hex_to_sdl_color("#a7f109", 255);
    SDL_Color main_color_2;         // Boarders and lines and text      = hex_to_sdl_color("#040500", 255);
    SDL_Color main_color_3;         // Background 2                     = hex_to_sdl_color("#d3e8a6", 255);    
    SDL_Color main_color_4;         // Accent color                     = hex_to_sdl_color("#0d26e4", 255);
    SDL_Color main_color_5;         // Accent color                     = hex_to_sdl_color("#e63a14", 255);
    SDL_Color main_color_6;         // Pale color                       = hex_to_sdl_color("#313131", 150);


    int basic_border_thickness_1;
    int basic_border_thickness_2;

    int basic_pixels_quantity_in_equivalent_unit;

    // Текущие масштабы для отображения (общее значение сигнала или времени на единицу сетки)
    int current_signal_scale;
    int current_time_scale;


    // Флаг для апдейта объектов GUI при смене настроек
    bool scope_gui_need_update;


    scope_gui_basic_parameters gui_parameters;                      // Данные для рендера фигур (в данной версии - задник, дисплей, сетка)
    signal_render_ctx signal_render_data;

    // Объекты (текстовые блоки)
    Textbox scope_signature_textbox;


    Textbox signal_scale_textbox;
    Textbox time_scale_textbox;

    Textbox amplitude_textbox;
    Textbox frequency_or_period_textbox;

    // Объекты (кнопки настройки + кнопка смены сигнала)
    Textbox change_value_scale_instruction_textbox;
    Button decrease_value_scale_button;
    Button increase_value_scale_button;

    Textbox change_time_scale_instruction_textbox;
    Button decrease_time_scale_button;
    Button increase_time_scale_button;

    Textbox change_frequency_instruction_textbox;
    Button decrease_frequency_button;
    Button increase_frequency_button;

    Textbox change_amplitude_instruction_textbox;
    Button decrease_amplitude_button;
    Button increase_amplitude_button;


    Button signal_change_button;

    Button mode_change_button;

    Button controlled_signal_play_button;


    Button scope_on_off_button;


} scope_render_ctx;


// ===== Scope render data =====


// =========================================================================================== SCOPE RENDERING


// =========================================================================================== SCOPE SETTINGS CTX

typedef struct scope_main_settings
{

    // Глобальные настройки
    scope_state current_state;                      // Текущий режим
    
    scope_render_mode current_mode;                 // Режим
    scope_render_mode acessable_modes[3];           // Доступные режимы (для итераций при переключениях)

    // Текущие единицы измерения для отображения
    signal_units current_signal_units;              // Всегда вольты
    time_units current_time_units;                  // Переменная времени всегда в секундах, но на рендер адекватнее выводить в другом формате
    frequncy_units current_frequency_units;         // Переменная времени всегда в герцах, но на рендер адекватнее выводить в другом формате

    // Сколько вольт в 1 юните (только целые числа для ровной развертки)
    int signal_val_in_one_unit;

    // Сколько времени в 1 юните (только целые числа для ровной развертки) - согласование с current_time_units
    int time_val_in_one_unit;

    // Количество периодов для отображения на 16 (в режиме с фикс. кол-вом)
    int periods_to_display;

} scope_main_settings_ctx;


// =========================================================================================== SCOPE SETTINGS CTX


// =========================================================================================== SCOPE DATA

typedef struct scope_signal_control_ctx
{
    
    // ===== MAIN SIGNAL DATA =====

    sin_generator_ctx* controlled_signal;                              // Контролируемый сигнал (в данной версии - только синус)
    controlled_signal_type type_of_controlled_signal;                  // Какой вид сигнала контролируем сейчас - КОСТЫЛЬ

    scope_buffer_ctx scope_buffer_data;                                // Буфер осциллографа

    // ===== MAIN SIGNAL DATA =====


    // ===== SIGNAL ANALYSIS DATA =====

    scope_running_signal_data_ctx running_signal_characteristics;      // Runtime-характеристики значения

    scope_measured_signal_data_ctx measured_signal_characteristics;    // Measured-характеристики значения

    // ===== SIGNAL ANALYSATORS DATA =====


    // ===== SIGNAL ANALYSATORS DATA =====

    scope_realtime_filtering_ctx filter_ctx;                           // Running-filter для running_signal_characteristics

    scope_realtime_peaks_ctx peaks_ctx;                                // Data peak-анализатора

    wave_pattern_detector_former_ctx wave_pattern_detector;            // Data детектора полуволн

    wave_pattern_detector_ctx halfwaves_for_check;                     // Буффер полуволн для проверки детектором паттернов / периода

    // ===== SIGNAL ANALYSATORS DATA =====

} scope_signal_control_ctx;

// =========================================================================================== SCOPE DATA


// =========================================================================================== SCOPE MAIN STRUCT

typedef struct Scope {

    scope_main_settings_ctx main_settings;           // Основные настройки осциллографа

    scope_signal_control_ctx signal_control_data;    // Данные контролируемого сигнала

    scope_render_ctx scope_render_data;              // Данные для рендеринга

} Scope;

// =========================================================================================== SCOPE MAIN STRUCT



// =========================================================================================== SCOPE API

// Инициализация осциллографа
void scope_init(Scope* used_scope, SDL_Renderer* renderer);


// Присвоение сигнала осциллографу (сеттер)
void signal_check(Scope* used_scope, sin_generator_ctx* controlled_signal);

// Апдейт общих характериктик системы каждый фрейм (получение
// runtime характеристик + отдача данных детектору полуволн / детектору периода)
// коллится без временных ограничений
void scope_fast_update(Scope* used_scope);

// Апдейт общих характериктик системы с частотой в 2-4 раза выше частоты кадров (обеспечивает
// получение адекватной даты) - частота 60 Гц
// берёт текущее значение сигнала
// записывает в кольцевой буфер
// фиксирует time + delta_t
// вызывает детектор событий (переход через 0)
void scope_slow_update(Scope* used_scope);


// Рендер с частотой соотв. текущей частоте кадров - частота 60 Гц
void scope_render(Scope* used_scope);


// Тут нет необходимости в сеттерах и геттерах, потому что структурное ООП - always public
// я просто иницирую дефолтный осциллограф, выделяю дефолтный буфер при инициализации, а затем
// присоединяю к нему сигнал, на update() осциллографа я получаю текущее значение сигнала и значение 
// тиков, на котором это время было снято и запихиваю оба значения в кольцевой буфер. На рендере я в
// зависимости от настроек масштабов развертки отрисовываю внутри дисплея нужный кусок буфера

void scope_destroy(Scope* used_scope);

// =========================================================================================== SCOPE API
