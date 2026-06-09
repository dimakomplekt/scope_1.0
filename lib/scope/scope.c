// scope.h


// =========================================================================================== IMPORT

#include "scope.h"

#include "../app_timer/app_timer.h"
#include "../sin_generator/sin_generator.h"

// =========================================================================================== IMPORT




// =========================================================================================== Helper-functions predeclare

void scope_buffer_init(scope_ctx* used_scope);

void scope_gui_init(scope_ctx* used_scope, SDL_Renderer* renderer);
void scope_gui_renew(scope_ctx* used_scope);


// =========================================================================================== Helper-functions predeclare


// =========================================================================================== API

void scope_init(scope_ctx* used_scope, SDL_Renderer* renderer)
{
    // Main data init

    used_scope->main_settings.current_mode = SCOPE_MODE_SCROLL_TO_LEFT;     // Базово - скролл
    used_scope->main_settings.periods_to_display = 2;                       // Базово - 2 периода для отображения (в режиме с фикс. кол-вом)
    used_scope->main_settings.current_signal_units  = VOLTS;                // Базово - вольты 
    used_scope->main_settings.current_time_units = MICROSECONDS;            // Базово - микросекунды

    scope_buffer_init(used_scope);

    // No signal at the start
    used_scope->signal_control_data.controlled_signal = NULL;

    // GUI init and instant renew to setup
    scope_gui_init(used_scope, renderer);
    scope_gui_renew(used_scope);
}


void signal_check(scope_ctx* used_scope, sin_generator_ctx* controlled_signal)
{

}


void scope_update(scope_ctx* used_scope)
{

}


void scope_render(scope_ctx* used_scope)
{

}


void scope_buffer_init(scope_ctx* used_scope)
{
    /*

    Очистить массивы времён и значений (не обязательно для работы кольцевого буфера, но удобно для отладки).

    Установить head = 0 — первая запись пойдёт в начало массива.

    Установить count = 0 — данных пока нет.

    */

    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    memset(buffer->timestamps, 0, sizeof(buffer->timestamps));
    memset(buffer->samples_values, 0, sizeof(buffer->samples_values));

    buffer->head = 0;
    buffer->count = 0;
}


void scope_gui_init(scope_ctx* used_scope, SDL_Renderer* renderer)
{
    // Инициализация рендерера
    used_scope->scope_render_data.renderer = renderer;


    // Строим осциллограф по сетке, используя 50 пикселей на единицу сетки 
    used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit = 50;


    // Базово - 1 вольт, 100 мкс на единицу сетки
    used_scope->scope_render_data.current_signal_scale = 1;
    used_scope->scope_render_data.current_time_scale = 100;

}


void scope_gui_renew(scope_ctx* used_scope)
{

}


void scope_destroy(scope_ctx* used_scope)
{

}

// =========================================================================================== API
