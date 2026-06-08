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


// ===== Scope render data =====

typedef struct scope_render {

    // Глобальные настройки

    scope_mode_en current_mode;                     // Режим

    int periods_to_display;                         // Количество периодов для отображения (в режиме с фикс. кол-вом)

    int basic_pixels_quantity_in_equivalent_unit;

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

    // Объекты (текстовые блоки)
    Textbox* scope_signature_textbox;

    Textbox* signal_value_0_textbox;
    Textbox* signal_value_1_textbox;
    Textbox* signal_value_2_textbox;
    Textbox* signal_value_3_textbox;
    Textbox* signal_value_4_textbox;
    Textbox* signal_value_5_textbox;
    Textbox* signal_value_6_textbox;
    Textbox* signal_value_7_textbox;


    Textbox* time_value_0_textbox;
    Textbox* time_value_1_textbox;
    Textbox* time_value_2_textbox;
    Textbox* time_value_3_textbox;
    Textbox* time_value_4_textbox;
    Textbox* time_value_5_textbox;
    Textbox* time_value_6_textbox;
    Textbox* time_value_7_textbox;


    Textbox* change_value_scale_instruction_textbox;

    Textbox* change_time_scale_instruction_textbox;

    Textbox* change_mode_instruction_textbox;

    Textbox* change_signal_instruction_textbox;


    // Объекты (кнопки настройки + кнопка смены сигнала)
    
    Button* decrease_value_scale_button;
    Button* increase_value_scale_button;

    Button* decrease_time_scale_button;
    Button* increase_time_scale_button;

    Button* change_mode_button;

    Button* change_signal_button;


} scope_render_ctx;


// ===== Scope =====

typedef struct scope {

    scope_buffer_ctx scope_buffer_data;             // Буфер осциллографа


    sin_generator_ctx* controlled_signal;           // Контролируемый сигнал (в данной версии - только синус)


    scope_render_ctx scope_render_data;             // Данные для рендеринга

} scope_ctx;


// =========================================================================================== SCOPE STRUCT


// =========================================================================================== INNER FUNCTIONS

void scope_init(scope_ctx* used_scope);

void connect_scope_sensor(scope_ctx* used_scope, sin_generator_ctx* controlled_signal);

void scope_update(scope_ctx* used_scope);

void scope_render(scope_ctx* used_scope, SDL_Renderer renderer);

void scope_gui_renew(scope_ctx* used_scope);

void scope_data_clear(scope_ctx* used_scope);

// Тут нет необходимости в сеттерах и геттерах, потому что структурное ООП - always public
// я просто иницирую дефолтный осциллограф, выделяю дефолтный буфер при инициализации, а затем
// присоединяю к нему сигнал, на update() осциллографа я получаю текущее значение сигнала и значение 
// тиков, на котором это время было снято и запихиваю оба значения в кольцевой буфер. На рендере я в
// зависимости от настроек масштабов развертки отрисовываю внутри дисплея нужный кусок буфера


// =========================================================================================== INNER FUNCTIONS
