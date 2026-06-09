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

typedef struct scope_base_gui_figures
{
    /*

        Данные для вызова функций рендера фигур (в данной версии - задник, дисплей, сетка)

        void my_sdl_draw_line(
            
            SDL_Renderer* renderer,
            int x1, int y1,
            int x2, int y2,
            SDL_Color color
                            
        );


        void my_sdl_draw_filled_rect(

            SDL_Renderer* renderer,
            int x, int y,
            int w, int h,
            SDL_Color fill_color,
            SDL_Color border_color,
            int border_thickness

        );

    */

    // ===== Задник =====

    int background_x;
    int background_y;

    int background_w;
    int background_h;

    SDL_Color background_fill_color;

    SDL_Color background_border_color;

    int background_border_thicknes;


    // ===== Дисплей =====

    int display_x;
    int display_y;

    int display_w;
    int display_h;

    SDL_Color display_fill_color;

    SDL_Color display_border_color;

    int display_border_thicknes;

    // ===== Сетка =====

    // Line 1

    int line_1_x1;
    int line_1_y1;

    int line_1_x2;
    int line_1_y2;

    SDL_Color line_1_color;


    // Line 2

    int line_2_x1;
    int line_2_y1;
    
    int line_2_x2;
    int line_2_y2;

    SDL_Color line_2_color;


    // Line 3

    int line_3_x1;
    int line_3_y1;
    
    int line_3_x2;
    int line_3_y2;

    SDL_Color line_3_color;


    // Line 4

    int line_4_x1;
    int line_4_y1;
    
    int line_4_x2;
    int line_4_y2;

    SDL_Color line_4_color;


    // Line 5

    int line_5_x1;
    int line_5_y1;
    
    int line_5_x2;
    int line_5_y2;

    SDL_Color line_5_color;


    // Line 6

    int line_6_x1;
    int line_6_y1;
    
    int line_6_x2;
    int line_6_y2;

    SDL_Color line_6_color;


    // Line 7

    int line_7_x1;
    int line_7_y1;
    
    int line_7_x2;
    int line_7_y2;

    SDL_Color line_7_color;


    // Line 8

    int line_8_x1;
    int line_8_y1;
    
    int line_8_x2;
    int line_8_y2;

    SDL_Color line_8_color;


    // Line 9

    int line_9_x1;
    int line_9_y1;
    
    int line_9_x2;
    int line_9_y2;

    SDL_Color line_9_color;


    // Line 10

    int line_10_x1;
    int line_10_y1;
    
    int line_10_x2;
    int line_10_y2;

    SDL_Color line_10_color;


    // Line 11

    int line_11_x1;
    int line_11_y1;
    
    int line_11_x2;
    int line_11_y2;

    SDL_Color line_11_color;


    // Line 12

    int line_12_x1;
    int line_12_y1;
    
    int line_12_x2;
    int line_12_y2;

    SDL_Color line_12_color;


    // Line 13

    int line_13_x1;
    int line_13_y1;
    
    int line_13_x2;
    int line_13_y2;

    SDL_Color line_13_color;


    // Line 14

    int line_14_x1;
    int line_14_y1;
    
    int line_14_x2;
    int line_14_y2;

    SDL_Color line_14_color;


    // Line 15

    int line_15_x1;
    int line_15_y1;
    
    int line_15_x2;
    int line_15_y2;

    SDL_Color line_15_color;


    // Line 16

    int line_16_x1;
    int line_16_y1;
    
    int line_16_x2;
    int line_16_y2;

    SDL_Color line_16_color;


} scope_base_gui_figures;



typedef struct scope_render {

    // Ссылка на рендерер
    SDL_Renderer* renderer;

    // Глобальные настройки

    int basic_pixels_quantity_in_equivalent_unit;

    // Текущие масштабы для отображения (общее значение сигнала или времени на единицу сетки)
    int current_signal_scale;
    int current_time_scale;

    // Базовые настройки отображения

    int width_units;
    int height_units;

    int screen_width_units;
    int screen_height_units;

    int buttons_signature_width_units;
    int buttons_signature_height_units;

    int buttons_width_units;
    int buttons_height_units;
    
    int margin_units;

    // Флаг для апдейта объектов GUI при смене настроек
    bool scope_gui_need_update;


    scope_base_gui_figures figures;                      // Данные для рендера фигур (в данной версии - задник, дисплей, сетка)

    // Объекты (текстовые блоки)
    Textbox scope_signature_textbox;

    Textbox signal_value_0_textbox;
    Textbox signal_value_1_textbox;
    Textbox signal_value_2_textbox;
    Textbox signal_value_3_textbox;
    Textbox signal_value_4_textbox;
    Textbox signal_value_5_textbox;
    Textbox signal_value_6_textbox;
    Textbox signal_value_7_textbox;

    Textbox signal_unit_textbox;


    Textbox time_value_0_textbox;
    Textbox time_value_1_textbox;
    Textbox time_value_2_textbox;
    Textbox time_value_3_textbox;
    Textbox time_value_4_textbox;
    Textbox time_value_5_textbox;
    Textbox time_value_6_textbox;
    Textbox time_value_7_textbox;

    Textbox time_unit_textbox;


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

    int zero_crossing_timestamps


    int current_max_signal_value;
    int current_min_signal_value;

} scope_signal_control_ctx;


// ===== Scope =====

typedef struct scope {

    scope_main_settings_ctx main_settings;           // Основные настройки осциллографа (в данной версии - режим отображения и кол-во периодов для отображения в режиме с фикс. кол-вом)

    scope_signal_control_ctx signal_control_data;   // Данные контролируемого сигнала

    scope_render_ctx scope_render_data;             // Данные для рендеринга

} scope_ctx;


// =========================================================================================== SCOPE STRUCT


// =========================================================================================== INNER FUNCTIONS

void scope_init(scope_ctx* used_scope, SDL_Renderer* renderer);

void signal_check(scope_ctx* used_scope, sin_generator_ctx* controlled_signal);

void scope_update(scope_ctx* used_scope);

void scope_render(scope_ctx* used_scope);


// Тут нет необходимости в сеттерах и геттерах, потому что структурное ООП - always public
// я просто иницирую дефолтный осциллограф, выделяю дефолтный буфер при инициализации, а затем
// присоединяю к нему сигнал, на update() осциллографа я получаю текущее значение сигнала и значение 
// тиков, на котором это время было снято и запихиваю оба значения в кольцевой буфер. На рендере я в
// зависимости от настроек масштабов развертки отрисовываю внутри дисплея нужный кусок буфера


void scope_destroy(scope_ctx* used_scope);

// =========================================================================================== INNER FUNCTIONS
