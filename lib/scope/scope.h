// scope.h

#pragma once

// =========================================================================================== IMPORT

#include "../engine.h"

#include "../sin_generator/sin_generator.h"

#include "../SDL2/UI_elements/my_sdl_button/my_sdl_button.h"
#include "../SDL2/UI_elements/my_sdl_textbox/my_sdl_textbox.h"

#include <stdbool.h>

// =========================================================================================== IMPORT


// =========================================================================================== SCOPE STRUCT


// ===== Buffer type =====

#define BUFFER_SIZE 60000

// Кольцевой буфер - пишем всегда в head, при переполнении начинаем перезаписывать старую дату, чтение
// производим для элементов, которые стоят до head (в случае переполнения при переходе head в [0] и 
// чтении [head - 1] получим чтение head[BUFFER_SIZE]) соотв. дата всегда будет адекватной без необходимости
// производить сдвиги значений

typedef struct scope_buffer {

    int32_t timestamps[BUFFER_SIZE];               // Времена сигналов

    // TODO: FLOAT OR X1000 ect..
    int16_t samples_values[BUFFER_SIZE];           // Значения сигналов

    int head;                                      // Текущий индекс (для записи и чтения)
    int count;                                     // Счётчик (для контроля переполнений)

} scope_buffer_ctx;


// ===== Scope mode enum =====

typedef enum scope_mode
{

    SCOPE_MODE_SCROLL_TO_LEFT,
    SCOPE_MODE_N_PERIODS

} scope_mode_en;


typedef enum signal_units
{

    VOLTS

} signal_units;



typedef enum time_units
{

    NANOSECONDS,
    MICROSECONDS,
    MILLISECONDS,
    SECONDS
    
} time_units;


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

    int background_border_thicknes_1;


    // Средняя часть

    int background_x_2;
    int background_y_2;

    int background_w_2;
    int background_h_2;

    SDL_Color background_fill_color_2;

    SDL_Color background_border_color_2;

    int background_border_thicknes_2;


    // Нижняя часть
    
    int background_x_3;
    int background_y_3;

    int background_w_3;
    int background_h_3;

    SDL_Color background_fill_color_3;

    SDL_Color background_border_color_3;

    int background_border_thicknes_3;


    // ===== Дисплей =====

    int display_x;
    int display_y;

    int display_w;
    int display_h;

    SDL_Color display_fill_color;

    SDL_Color display_border_color;

    int display_border_thicknes;


    // ===== Сетка =====

    // 8 горизонтальных линий + 16 вертикальных линий для сетки

    int lines_thicknes;

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

    int signal_scale_set_info_x_1;
    int signal_scale_set_info_y_1;

    int signal_scale_set_info_w_1;
    int signal_scale_set_info_h_1;

    SDL_Color signal_scale_set_info_fill_color_1;

    SDL_Color signal_scale_set_info_border_color_1;

    int signal_scale_set_info_border_thicknes_1;

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

    int time_scale_set_info_border_thicknes_1;


    // Координаты кнопки уменьшения масштаба времени
    int time_scale_decrease_button_x_1;
    int time_scale_decrease_button_y_1;
    
    // Координаты кнопки увеличения масштаба времени

    int time_scale_increase_button_x_1;
    int time_scale_increase_button_y_1;


    // Прямоугольник пояснения и координаты для текстбокса инструкции по изменению частоты сигнала

    int freq_reset_info_x_1;
    int freq_reset_info_y_1;

    int freq_reset_info_w_1;
    int freq_reset_info_h_1;

    SDL_Color freq_reset_info_fill_color_1;

    SDL_Color freq_reset_info_border_color_1;

    int freq_info_border_thicknes_1;


    // Координаты кнопки уменьшения масштаба времени
    int freq_decrease_button_x_1;
    int freq_decrease_button_y_1;
    
    // Координаты кнопки увеличения масштаба времени

    int freq_increase_button_x_1;
    int freq_increase_button_y_1;


    // Координаты кнопки смены режима отображения
    int image_regime_change_button_x_1;
    int image_regime_change_button_y_1;
    

    // Координаты кнопки смены сигнала

    int signal_change_button_x_1;
    int signal_change_button_y_1;


    // Прямоугольник для масштаба сигнала

    int signal_scale_info_x_1;
    int signal_scale_info_y_1;

    int signal_scale_info_w_1;
    int signal_scale_info_h_1;

    SDL_Color signal_scale_info_fill_color_1;

    SDL_Color signal_scale_info_border_color_1;

    int signal_scale_info_border_thicknes_1;


    // Прямоугольник для масштаба времени

    int time_scale_info_x_1;
    int time_scale_info_y_1;

    int time_scale_info_w_1;
    int time_scale_info_h_1;

    SDL_Color time_scale_info_fill_color_1;

    SDL_Color time_scale_info_border_color_1;

    int time_scale_info_border_thicknes_1;


    // Прямоугольник для амплитуды сигнала

    int amplitude_info_x_1;
    int amplitude_info_y_1;

    int amplitude_info_w_1;
    int amplitude_info_h_1;

    SDL_Color amplitude_info_fill_color_1;

    SDL_Color amplitude_info_border_color_1;

    int amplitude_info_border_thicknes_1;



    // Прямоугольник для частоты сигнала

    int frequency_info_x_1;
    int frequency_info_y_1;

    int frequency_info_w_1;
    int frequency_info_h_1;

    SDL_Color frequency_info_fill_color_1;

    SDL_Color frequency_info_border_color_1;

    int frequency_info_border_thicknes_1;


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
    SDL_Color main_color_5;         // Pale color                       = hex_to_sdl_color("#313131", 150);


    int basic_border_thicknes_1;
    int basic_border_thicknes_2;

    int basic_pixels_quantity_in_equivalent_unit;

    // Текущие масштабы для отображения (общее значение сигнала или времени на единицу сетки)
    int current_signal_scale;
    int current_time_scale;


    // Флаг для апдейта объектов GUI при смене настроек
    bool scope_gui_need_update;


    scope_gui_basic_parameters gui_parameters;                      // Данные для рендера фигур (в данной версии - задник, дисплей, сетка)


    // Объекты (текстовые блоки)
    Textbox scope_signature_textbox;


    Textbox signal_scale_textbox;
    Textbox time_scale_textbox;
    Textbox frequency_textbox;
    Textbox amplitude_textbox;


    // Объекты (кнопки настройки + кнопка смены сигнала)
    Textbox change_value_scale_instruction_textbox;
    Button decrease_value_scale_button;
    Button increase_value_scale_button;

    Textbox change_time_scale_instruction_textbox;
    Button decrease_time_scale_button;
    Button increase_time_scale_button;

    Textbox change_freq_instruction_textbox;
    Button decrease_freq_button;
    Button increase_freq_button;

    Textbox change_mode_instruction_textbox;
    Button change_mode_button;

    Textbox change_signal_instruction_textbox;
    Button change_signal_button;


} scope_render_ctx;


typedef struct scope_main_settings
{

    // Глобальные настройки

    scope_mode_en current_mode;                     // Режим

    int periods_to_display;                         // Количество периодов для отображения (в режиме с фикс. кол-вом)

    // Текущие единицы измерения для отображения
    signal_units current_signal_units;
    time_units current_time_units;

} scope_main_settings_ctx;


typedef struct scope_signal_control_ctx
{

    sin_generator_ctx* controlled_signal;           // Контролируемый сигнал (в данной версии - только синус)

    scope_buffer_ctx scope_buffer_data;             // Буфер осциллографа

    // ==== Анализ сигнала для рескейла дисплея в моде с фикс. кол-вом периодов ====

    // Данные о периоде
    int current_period_value;
    int current_frequency_value;

    // TODO!!!
    int zero_crossing_timestamps;


    int current_max_signal_value;
    int current_min_signal_value;

} scope_signal_control_ctx;


// ===== Scope =====

typedef struct Scope {

    scope_main_settings_ctx main_settings;           // Основные настройки осциллографа (в данной версии - режим отображения и кол-во периодов для отображения в режиме с фикс. кол-вом)

    scope_signal_control_ctx signal_control_data;   // Данные контролируемого сигнала

    scope_render_ctx scope_render_data;             // Данные для рендеринга

} Scope;


// =========================================================================================== SCOPE STRUCT


// =========================================================================================== INNER FUNCTIONS

void scope_init(Scope* used_scope, SDL_Renderer* renderer);

void signal_check(Scope* used_scope, sin_generator_ctx* controlled_signal);

void scope_update(Scope* used_scope);

void scope_render(Scope* used_scope);


// Тут нет необходимости в сеттерах и геттерах, потому что структурное ООП - always public
// я просто иницирую дефолтный осциллограф, выделяю дефолтный буфер при инициализации, а затем
// присоединяю к нему сигнал, на update() осциллографа я получаю текущее значение сигнала и значение 
// тиков, на котором это время было снято и запихиваю оба значения в кольцевой буфер. На рендере я в
// зависимости от настроек масштабов развертки отрисовываю внутри дисплея нужный кусок буфера


void scope_destroy(Scope* used_scope);

// =========================================================================================== INNER FUNCTIONS
