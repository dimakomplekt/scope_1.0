// scope.h


// =========================================================================================== IMPORT

#include "scope.h"

#include <stdlib.h>

#include "../app_timer/app_timer.h"
#include "../sin_generator/sin_generator.h"


#include "../../src/global_data.h"

// =========================================================================================== IMPORT




// =========================================================================================== Helper-functions predeclare

void scope_buffer_init(Scope* used_scope);

void scope_gui_init(Scope* used_scope, SDL_Renderer* renderer);
void scope_gui_renew(Scope* used_scope);
void scope_display_animation(Scope* used_scope);

// =========================================================================================== Helper-functions predeclare


// =========================================================================================== API

void scope_init(Scope* used_scope, SDL_Renderer* renderer)
{
    // Main data init

    used_scope->main_settings.current_mode = SCOPE_MODE_SCROLL_TO_LEFT;     // Базово - скролл (синус инициируется низкочастотным)
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


void signal_check(Scope* used_scope, sin_generator_ctx* controlled_signal)
{
    //
}


void scope_update(Scope* used_scope)
{
    scope_display_animation(used_scope);

    Textbox_update(&used_scope->scope_render_data.scope_signature_textbox, used_scope->scope_render_data.renderer);
}


void scope_render(Scope* used_scope)
{
    // Background 0 (TEST!!!)
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.background_x_2,
        used_scope->scope_render_data.gui_parameters.background_y_2,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        used_scope->scope_render_data.main_color_3,
        used_scope->scope_render_data.gui_parameters.background_border_thicknes_1,
        used_scope->scope_render_data.main_color_3

    );


    // Background 1
    my_sdl_draw_filled_rect_bi(

        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.background_x_1,
        used_scope->scope_render_data.gui_parameters.background_y_1,
        used_scope->scope_render_data.gui_parameters.background_w_1,
        used_scope->scope_render_data.gui_parameters.background_h_1,
        used_scope->scope_render_data.gui_parameters.background_fill_color_1,
        used_scope->scope_render_data.gui_parameters.background_border_thicknes_1,
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
        used_scope->scope_render_data.gui_parameters.background_border_thicknes_2,
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
        used_scope->scope_render_data.gui_parameters.background_border_thicknes_2,
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
        used_scope->scope_render_data.gui_parameters.display_border_thicknes,
        used_scope->scope_render_data.gui_parameters.display_border_color

    );


    // Mesh

    // Горизонтальные линии 2 - 8

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_2_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_2_y1,
        used_scope->scope_render_data.gui_parameters.h_line_2_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_2_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_2_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_3_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_3_y1,
        used_scope->scope_render_data.gui_parameters.h_line_3_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_3_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_3_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_4_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_4_y1,
        used_scope->scope_render_data.gui_parameters.h_line_4_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_4_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_4_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_5_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_5_y1,
        used_scope->scope_render_data.gui_parameters.h_line_5_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_5_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_5_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_6_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_6_y1,
        used_scope->scope_render_data.gui_parameters.h_line_6_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_6_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_6_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_7_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_7_y1,
        used_scope->scope_render_data.gui_parameters.h_line_7_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_7_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_7_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.h_line_8_x1, 
        used_scope->scope_render_data.gui_parameters.h_line_8_y1,
        used_scope->scope_render_data.gui_parameters.h_line_8_x2, 
        used_scope->scope_render_data.gui_parameters.h_line_8_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.h_line_8_color
                        
    );


    // Вертикальные линии 2 - 16

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_2_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_2_y1,
        used_scope->scope_render_data.gui_parameters.v_line_2_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_2_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_2_color
                        
    );


    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_3_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_3_y1,
        used_scope->scope_render_data.gui_parameters.v_line_3_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_3_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_3_color
                        
    );


    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_4_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_4_y1,
        used_scope->scope_render_data.gui_parameters.v_line_4_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_4_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_4_color
                        
    );


    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_5_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_5_y1,
        used_scope->scope_render_data.gui_parameters.v_line_5_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_5_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_5_color
                        
    );


    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_6_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_6_y1,
        used_scope->scope_render_data.gui_parameters.v_line_6_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_6_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_6_color
                        
    );


    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_7_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_7_y1,
        used_scope->scope_render_data.gui_parameters.v_line_7_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_7_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_7_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_8_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_8_y1,
        used_scope->scope_render_data.gui_parameters.v_line_8_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_8_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_8_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_9_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_9_y1,
        used_scope->scope_render_data.gui_parameters.v_line_9_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_9_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_9_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_10_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_10_y1,
        used_scope->scope_render_data.gui_parameters.v_line_10_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_10_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_10_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_11_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_11_y1,
        used_scope->scope_render_data.gui_parameters.v_line_11_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_11_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_11_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_12_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_12_y1,
        used_scope->scope_render_data.gui_parameters.v_line_12_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_12_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_12_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_13_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_13_y1,
        used_scope->scope_render_data.gui_parameters.v_line_13_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_13_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_13_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_14_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_14_y1,
        used_scope->scope_render_data.gui_parameters.v_line_14_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_14_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_14_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_15_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_15_y1,
        used_scope->scope_render_data.gui_parameters.v_line_15_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_15_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_15_color
                        
    );

    my_sdl_draw_line(
        
        used_scope->scope_render_data.renderer,
        used_scope->scope_render_data.gui_parameters.v_line_16_x1, 
        used_scope->scope_render_data.gui_parameters.v_line_16_y1,
        used_scope->scope_render_data.gui_parameters.v_line_16_x2, 
        used_scope->scope_render_data.gui_parameters.v_line_16_y2,
        used_scope->scope_render_data.gui_parameters.lines_thicknes,
        used_scope->scope_render_data.gui_parameters.v_line_16_color
                        
    );
    
}


void scope_buffer_init(Scope* used_scope)
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
    used_scope->scope_render_data.main_color_5 = hex_to_sdl_color("#313131", 150);

    used_scope->scope_render_data.basic_border_thicknes_1 = 10;
    used_scope->scope_render_data.basic_border_thicknes_2 = 5;

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

    used_scope->scope_render_data.gui_parameters.background_border_thicknes_2 = used_scope->scope_render_data.basic_border_thicknes_1;


    // BG 1 - верхний задник, на котором располагается название дисплея, сидит сверху с отступом в 0.5 единиц (центр-центр) от верхнего края экрана

    used_scope->scope_render_data.gui_parameters.background_x_1 = used_scope->scope_render_data.x_position;

    used_scope->scope_render_data.gui_parameters.background_y_1 = (used_scope->scope_render_data.y_position - 
        (used_scope->scope_render_data.gui_parameters.background_h_2 + 
        used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit) * 0.5) + used_scope->scope_render_data.gui_parameters.background_border_thicknes_2;


    used_scope->scope_render_data.gui_parameters.background_w_1 = used_scope->scope_render_data.gui_parameters.background_w_2;

    used_scope->scope_render_data.gui_parameters.background_h_1 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.bg_1_height_units);


    used_scope->scope_render_data.gui_parameters.background_fill_color_1 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.background_border_color_1 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.background_border_thicknes_1 = used_scope->scope_render_data.basic_border_thicknes_1;

    // Текстбокс с названием дисплея
    used_scope->scope_render_data.scope_signature_textbox = *Textbox_init(used_scope->scope_render_data.main_color_2, 12);
    used_scope->scope_render_data.scope_signature_textbox.x = used_scope->scope_render_data.gui_parameters.background_x_1;
    used_scope->scope_render_data.scope_signature_textbox.y = used_scope->scope_render_data.gui_parameters.background_y_1;
    Textbox_set_content(&used_scope->scope_render_data.scope_signature_textbox, "SCOPE SCOPE SCOPE SCOPE");


    // BG 2 - нижний задник

    used_scope->scope_render_data.gui_parameters.background_x_3 = used_scope->scope_render_data.x_position;

    used_scope->scope_render_data.gui_parameters.background_y_3 = (used_scope->scope_render_data.y_position + 
        (used_scope->scope_render_data.gui_parameters.background_h_2 + 
        used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit) * 0.5) - used_scope->scope_render_data.gui_parameters.background_border_thicknes_2;


    used_scope->scope_render_data.gui_parameters.background_w_3 = used_scope->scope_render_data.gui_parameters.background_w_2;

    used_scope->scope_render_data.gui_parameters.background_h_3 = 
        (used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit * 
        used_scope->scope_render_data.gui_parameters.bg_3_height_units);


    used_scope->scope_render_data.gui_parameters.background_fill_color_3 = used_scope->scope_render_data.main_color_1;

    used_scope->scope_render_data.gui_parameters.background_border_color_3 = used_scope->scope_render_data.main_color_2;

    used_scope->scope_render_data.gui_parameters.background_border_thicknes_3 = used_scope->scope_render_data.basic_border_thicknes_1;


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

    used_scope->scope_render_data.gui_parameters.display_border_thicknes = used_scope->scope_render_data.basic_border_thicknes_2;

    used_scope->scope_render_data.gui_parameters.lines_thicknes = used_scope->scope_render_data.gui_parameters.display_border_thicknes * 0.5;
    
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

    used_scope->scope_render_data.gui_parameters.h_line_1_x1 = scope_low_border_x_1;
    used_scope->scope_render_data.gui_parameters.h_line_1_y1 = scope_low_border_y_1;

    used_scope->scope_render_data.gui_parameters.h_line_1_x2 = scope_low_border_x_2 - used_scope->scope_render_data.basic_border_thicknes_2;
;
    used_scope->scope_render_data.gui_parameters.h_line_1_y2 = scope_low_border_y_2;

    used_scope->scope_render_data.gui_parameters.h_line_1_color = used_scope->scope_render_data.main_color_2;


    // H Line 2 - смещение на 1 margin вверх относительно 1 линии

    used_scope->scope_render_data.gui_parameters.h_line_2_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_2_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 1 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_2_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_2_y2 = used_scope->scope_render_data.gui_parameters.h_line_2_y1;

    used_scope->scope_render_data.gui_parameters.h_line_2_color = used_scope->scope_render_data.main_color_2;


    // H Line 3 - по аналогии

    used_scope->scope_render_data.gui_parameters.h_line_3_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_3_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 2 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_3_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_3_y2 = used_scope->scope_render_data.gui_parameters.h_line_3_y1;

    used_scope->scope_render_data.gui_parameters.h_line_3_color = used_scope->scope_render_data.main_color_2;


    // H Line 4

    used_scope->scope_render_data.gui_parameters.h_line_4_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_4_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 3 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_4_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_4_y2 = used_scope->scope_render_data.gui_parameters.h_line_4_y1;

    used_scope->scope_render_data.gui_parameters.h_line_4_color = used_scope->scope_render_data.main_color_2;


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

    used_scope->scope_render_data.gui_parameters.h_line_6_color = used_scope->scope_render_data.main_color_2;


    // H Line 7

    used_scope->scope_render_data.gui_parameters.h_line_7_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_7_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 6 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_7_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_7_y2 = used_scope->scope_render_data.gui_parameters.h_line_7_y1;

    used_scope->scope_render_data.gui_parameters.h_line_7_color = used_scope->scope_render_data.main_color_2;


    // H Line 8

    used_scope->scope_render_data.gui_parameters.h_line_8_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_8_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 7 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_8_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_8_y2 = used_scope->scope_render_data.gui_parameters.h_line_8_y1;

    used_scope->scope_render_data.gui_parameters.h_line_8_color = used_scope->scope_render_data.main_color_2;


    // H Line 9

    used_scope->scope_render_data.gui_parameters.h_line_9_x1 = used_scope->scope_render_data.gui_parameters.h_line_1_x1;
    used_scope->scope_render_data.gui_parameters.h_line_9_y1 = used_scope->scope_render_data.gui_parameters.h_line_1_y1 - 8 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.h_line_9_x2 = used_scope->scope_render_data.gui_parameters.h_line_1_x2;
    used_scope->scope_render_data.gui_parameters.h_line_9_y2 = used_scope->scope_render_data.gui_parameters.h_line_9_y1;

    used_scope->scope_render_data.gui_parameters.h_line_9_color = used_scope->scope_render_data.main_color_2;


    // V Line 1

    used_scope->scope_render_data.gui_parameters.v_line_1_x1 = scope_left_border_x_1;
    used_scope->scope_render_data.gui_parameters.v_line_1_y1 = scope_left_border_y_1 - used_scope->scope_render_data.basic_border_thicknes_2;

    used_scope->scope_render_data.gui_parameters.v_line_1_x2 = scope_left_border_x_2;
    used_scope->scope_render_data.gui_parameters.v_line_1_y2 = scope_left_border_y_2;

    used_scope->scope_render_data.gui_parameters.v_line_1_color = used_scope->scope_render_data.main_color_2;


    // V Line 2

    used_scope->scope_render_data.gui_parameters.v_line_2_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 1 * margin_in_pixels;
    used_scope->scope_render_data.gui_parameters.v_line_2_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_2_x2 = used_scope->scope_render_data.gui_parameters.v_line_2_x1;
    used_scope->scope_render_data.gui_parameters.v_line_2_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_2_color = used_scope->scope_render_data.main_color_2;


    // V Line 3

    used_scope->scope_render_data.gui_parameters.v_line_3_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 2 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_3_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_3_x2 = used_scope->scope_render_data.gui_parameters.v_line_3_x1;
    used_scope->scope_render_data.gui_parameters.v_line_3_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_3_color = used_scope->scope_render_data.main_color_2;


    // V Line 4

    used_scope->scope_render_data.gui_parameters.v_line_4_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 3 * margin_in_pixels;


    used_scope->scope_render_data.gui_parameters.v_line_4_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_4_x2 = used_scope->scope_render_data.gui_parameters.v_line_4_x1;
    used_scope->scope_render_data.gui_parameters.v_line_4_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_4_color = used_scope->scope_render_data.main_color_2;


    // V Line 5

    used_scope->scope_render_data.gui_parameters.v_line_5_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 4 * margin_in_pixels;


    used_scope->scope_render_data.gui_parameters.v_line_5_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_5_x2 = used_scope->scope_render_data.gui_parameters.v_line_5_x1;
    used_scope->scope_render_data.gui_parameters.v_line_5_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_5_color = used_scope->scope_render_data.main_color_2;


    // V Line 6

    used_scope->scope_render_data.gui_parameters.v_line_6_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 5 * margin_in_pixels;

    used_scope->scope_render_data.gui_parameters.v_line_6_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_6_x2 = used_scope->scope_render_data.gui_parameters.v_line_6_x1;
    used_scope->scope_render_data.gui_parameters.v_line_6_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_6_color = used_scope->scope_render_data.main_color_2;


    // V Line 7

    used_scope->scope_render_data.gui_parameters.v_line_7_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 6 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_7_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_7_x2 = used_scope->scope_render_data.gui_parameters.v_line_7_x1;
    used_scope->scope_render_data.gui_parameters.v_line_7_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_7_color = used_scope->scope_render_data.main_color_2;


    // V Line 8

    used_scope->scope_render_data.gui_parameters.v_line_8_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 7 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_8_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_8_x2 = used_scope->scope_render_data.gui_parameters.v_line_8_x1;
    used_scope->scope_render_data.gui_parameters.v_line_8_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_8_color = used_scope->scope_render_data.main_color_2;


    // V Line 9

    used_scope->scope_render_data.gui_parameters.v_line_9_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 8 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_9_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_9_x2 = used_scope->scope_render_data.gui_parameters.v_line_9_x1;
    used_scope->scope_render_data.gui_parameters.v_line_9_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_9_color = used_scope->scope_render_data.main_color_2;


    // V Line 10

    used_scope->scope_render_data.gui_parameters.v_line_10_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 9 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_10_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_10_x2 = used_scope->scope_render_data.gui_parameters.v_line_10_x1;
    used_scope->scope_render_data.gui_parameters.v_line_10_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_10_color = used_scope->scope_render_data.main_color_2;


    // V Line 11

    used_scope->scope_render_data.gui_parameters.v_line_11_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 10 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_11_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_11_x2 = used_scope->scope_render_data.gui_parameters.v_line_11_x1;
    used_scope->scope_render_data.gui_parameters.v_line_11_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_11_color = used_scope->scope_render_data.main_color_2;


    // V Line 12

    used_scope->scope_render_data.gui_parameters.v_line_12_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 11 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_12_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_12_x2 = used_scope->scope_render_data.gui_parameters.v_line_12_x1;
    used_scope->scope_render_data.gui_parameters.v_line_12_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_12_color = used_scope->scope_render_data.main_color_2;


    // V Line 13

    used_scope->scope_render_data.gui_parameters.v_line_13_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 12 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_13_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_13_x2 = used_scope->scope_render_data.gui_parameters.v_line_13_x1;
    used_scope->scope_render_data.gui_parameters.v_line_13_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_13_color = used_scope->scope_render_data.main_color_2;


    // V Line 14

    used_scope->scope_render_data.gui_parameters.v_line_14_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 13 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_14_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_14_x2 = used_scope->scope_render_data.gui_parameters.v_line_14_x1;
    used_scope->scope_render_data.gui_parameters.v_line_14_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_14_color = used_scope->scope_render_data.main_color_2;


    // V Line 15

    used_scope->scope_render_data.gui_parameters.v_line_15_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 14 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_15_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_15_x2 = used_scope->scope_render_data.gui_parameters.v_line_15_x1;
    used_scope->scope_render_data.gui_parameters.v_line_15_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_15_color = used_scope->scope_render_data.main_color_2;


    // V Line 16

    used_scope->scope_render_data.gui_parameters.v_line_16_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 15 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_16_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_16_x2 = used_scope->scope_render_data.gui_parameters.v_line_16_x1;
    used_scope->scope_render_data.gui_parameters.v_line_16_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_16_color = used_scope->scope_render_data.main_color_2;


    // V Line 17

    used_scope->scope_render_data.gui_parameters.v_line_17_x1 = used_scope->scope_render_data.gui_parameters.v_line_1_x1 + 16 * margin_in_pixels;
    
    used_scope->scope_render_data.gui_parameters.v_line_17_y1 = used_scope->scope_render_data.gui_parameters.v_line_1_y1;
    
    used_scope->scope_render_data.gui_parameters.v_line_17_x2 = used_scope->scope_render_data.gui_parameters.v_line_17_x1;
    used_scope->scope_render_data.gui_parameters.v_line_17_y2 = used_scope->scope_render_data.gui_parameters.v_line_1_y2;

    used_scope->scope_render_data.gui_parameters.v_line_17_color = used_scope->scope_render_data.main_color_2;

}

// Изменение цвета дисплея - дребезжание
void scope_display_animation(Scope* used_scope)
{

    SDL_Color base_color = used_scope->scope_render_data.main_color_3;

    int noise_amplitude = 4;

    SDL_Color noised_color;

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

    used_scope->scope_render_data.gui_parameters.display_fill_color =
        noised_color;
}

void scope_destroy(Scope* used_scope)
{

}

// =========================================================================================== API
