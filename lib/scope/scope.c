// scope.h


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

void zero_crossings_check(Scope* used_scope);                       // Вспомогательная функция внутри void scope_buffer_update(Scope* used_scope);
void scope_signal_buffer_init(Scope* used_scope);                   // Инициализация буфера осциллографа ДИНАМИЧЕСКОЕ ВЫДЕЛЕНИЕ !!!
void scope_zero_cross_buffer_init(Scope* used_scope);                      // Инициализация буфера переходов через 0

void scope_gui_init(Scope* used_scope, SDL_Renderer* renderer);     // Инициализация графики осциллографа
void scope_gui_renew(Scope* used_scope);                            // Обновление графики осциллографа

void scope_screen_gui_init(Scope* used_scope);                      // Обновление графики экрана осциллографа ДИНАМИЧЕСКОЕ ВЫДЕЛЕНИЕ !!!
void scope_screen_gui_delete(Scope* used_scope);                    // Удаление буффера текущей отрисовки (при рескейле может понадобится)


// signal -> O(1)
// zero crossing -> O(1)
// period -> O(K), где K = 4–16 событий
// amplitude -> O(window)
void scope_find_period(Scope* used_scope);                          // Рассчёт периода
void scope_find_amplitude(Scope* used_scope);                       // Рассчёт амплитуды
void scope_screens_gui_renew_by_signal_data(Scope* used_scope);     // Обновление данных для рендера сигнала 

void build_fixed_time_render(Scope* used_scope, signal_render_ctx* render_data);


void scope_signal_info_gui_renew(Scope* used_scope);                // Обновление текстбоксов в дисплее информации
void scope_display_animation(Scope* used_scope);                    // Расчёт анимации мерцания дисплеев
void draw_signal(Scope* used_scope, SDL_Renderer* renderer);         // Функция рендеринга сигнала

void scope_buffer_clear(Scope* used_scope);                         // Очистка данных буффера при выключении


// Buttons callbacks

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


// =========================================================================================== Helper-functions predeclare


// =========================================================================================== API

void scope_init(Scope* used_scope, SDL_Renderer* renderer)
{
    // Main data init

    used_scope->main_settings.current_state = OFF_SS;
    used_scope->main_settings.current_mode = SCOPE_MODE_FIXED_TIME_STEP_SRM;     // Базово - скролл (синус инициируется низкочастотным)
    
    used_scope->main_settings.acessable_modes[0] = SCOPE_MODE_FIXED_TIME_STEP_SRM;
    used_scope->main_settings.acessable_modes[1] = SCOPE_MODE_SCROLL_TO_RIGHT_SRM;
    used_scope->main_settings.acessable_modes[2] = LIMIT_SRM;

    used_scope->main_settings.periods_to_display = 2;                           // Базово - 2 периода для отображения (в режиме с фикс. кол-вом)
    used_scope->main_settings.time_val_in_one_unit = 1;                         // Базово - 1 (режим с фикс. разв)
    used_scope->main_settings.signal_val_in_one_unit = 1;                       // Базово - 1 (режим с фикс. разв)
    
    used_scope->main_settings.current_signal_units  = VOLTS_SU;                 // Базово - вольты 
    used_scope->main_settings.current_time_units = MILLISECONDS_TU;             // Базово - микросекунды (но переменная всегда в секундах)
    used_scope->main_settings.current_frequency_units = HERTZ_FU;               // Базово - Герцы (но переменная всегда в Герцах)

    // Инициализация буффера
    scope_signal_buffer_init(used_scope);
    scope_zero_cross_buffer_init(used_scope);

    // No signal at the start
    used_scope->signal_control_data.controlled_signal = NULL;

    // GUI init and instant renew to setup
    scope_gui_init(used_scope, renderer);
    scope_gui_renew(used_scope);
    scope_screen_gui_init(used_scope);
}


void signal_check(Scope* used_scope, sin_generator_ctx* controlled_signal)
{
    // Подключаем сигнал к осциллографу
    if (!used_scope) return;
    if (!controlled_signal) return;

    used_scope->signal_control_data.controlled_signal = controlled_signal;
    used_scope->signal_control_data.type_of_controlled_signal = CLEAN_CST;

    used_scope->signal_control_data.controlled_signal->current_treshold = ZC_THRESHOLD_START_VALUE;

    
    used_scope->signal_control_data.threshold_update_accumulator = 0.0f;
    used_scope->signal_control_data.amplitude_estimate = 0;
    used_scope->signal_control_data.amplitude_initialized = false;
}


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
    

    // update ring buffer
    buffer->head = (head + 1) % BUFFER_SIZE;

    if (buffer->count < BUFFER_SIZE) buffer->count++;

    // Чек пересечений
    zero_crossings_check(used_scope);
}


void buffer_analysis(Scope* used_scope)
{

    /*
        3 вида показа typedef enum scope_state { OFF_SS, ON_SS, LIMIT_SS } scope_state; 
        
        typedef enum scope_render_mode 
        { SCOPE_MODE_FIXED_TIME_STEP_SRM, 
         SCOPE_MODE_SCROLL_TO_RIGHT_SRM, 
         SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM, 
         LIMIT_SRM } scope_render_mode; 
         
         typedef enum signal_units { VOLTS_SU, LIMIT_SU } 
         
         signal_units; typedef enum time_units { NANOSECONDS_TU, MICROSECONDS_TU, MILLISECONDS_TU, SECONDS_TU, LIMIT_TU } time_units; 
         первично на ините выставлен первый мод такой void scope_init(Scope* used_scope, SDL_Renderer* renderer)
          { // Main data init used_scope->main_settings.current_state = OFF_SS; 
           used_scope->main_settings.current_mode = SCOPE_MODE_SCROLL_TO_RIGHT_SRM; 
           // Базово - скролл (синус инициируется низкочастотным) used_scope->main_settings.periods_to_display = 2; 
           // Базово - 2 периода для отображения (в режиме с фикс. кол-вом) 
           used_scope->main_settings.current_signal_units = VOLTS_SU; // Базово - вольты 
           used_scope->main_settings.current_time_units = MICROSECONDS_TU; // Базово - микросекунды 
           
           у меня есть функция которая коллится в 4 раза чаще рендера void buffer_analysis(Scope* used_scope) 
           { if (used_scope->main_settings.current_state == ON_SS) 
            { // Анализируем буффер - смотрим на последнюю полученную дату в буффере и фиксируем характеристики
              // сигнала там же присваиваем новый content текстбоксам характеристики 
              // Find period } } в ней можно по буфферу найти каким-то неводомым мне образом основные параметры
               сигнала и записать их в 
               
               typedef struct scope_signal_control_ctx { sin_generator_ctx* controlled_signal; // Контролируемый сигнал (в данной версии 
               - только синус) 
               
               controlled_signal_type type_of_controlled_signal; // Какой вид сигнала контролируем сейчас 
               scope_buffer_ctx scope_buffer_data; // Буфер осциллографа 
               
               // ==== Анализ сигнала для рескейла дисплея в моде с фикс. кол-вом периодов ==== 
               
               // Текущие данные о сигнале 
               int current_period_value; 
               int current_frequency_value; 
               int current_max_signal_value; 
               int current_min_signal_value; } scope_signal_control_ctx; 
               
               а дальше подготовить по текущим флагам из 
               
               typedef struct scope_main_settings { 
               
               // Глобальные настройки scope_state current_state; 
               // Текущий режим scope_render_mode current_mode; 
               // Режим int periods_to_display; // Количество периодов для отображения (в режиме с фикс. кол-вом) 
               // Текущие единицы измерения для отображения 
               signal_units current_signal_units; 
               time_units current_time_units; } scope_main_settings_ctx; 
               
               и текущему буфферу (пользуясь требуемым количеством последних значений под дисплей) зная, 
               что у меня дисплей с размерами 
               
               used_scope->scope_render_data.gui_parameters.display_w 
               used_scope->scope_render_data.gui_parameters.display_h 
               
               центр которого сидит в точке 
               
               used_scope->scope_render_data.gui_parameters.display_x 
               used_scope->scope_render_data.gui_parameters.display_y 
               
               подготовить по текущему масштабу одной клетки размером (такая же и по x - время и по y - значение оси) 
               used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit 
               Какой-то signal_render_ctx 
               с 
               signal_render_points[used_scope->scope_render_data.gui_parameters.display_w] 
               
               int x; 
               int y; 
               bool show 
               
               который я потом отрисую через итератор по этому контексту и команду 
               
               void my_sdl_draw_pixel( SDL_Renderer* renderer, int x, int y, SDL_Color color ) 
               
               { SDL_SetRenderDrawColor(renderer, color.render_buffer, color.g, color.b, color.a); 
                SDL_RenderDrawPoint(renderer, x, y); }
                
                может быть даже через несколько команд (типо ещё сверху и снизу рисовать points_to_draw_quantity пикселей 
                для толстой линии сигнала сдвигом по y)
    
    */
    if (!used_scope) return;

    if (used_scope->main_settings.current_state != ON_SS) return;


    scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
    scope_buffer_ctx* buffer = &ctrl->scope_buffer_data;

    // Мин. кол-во даты для анализа

    bool update_data_skip_flag = true;
    if (buffer->count > 2) update_data_skip_flag = false;

    if (!update_data_skip_flag)
    {

        // Вычисляем период и частоту пользуясь буффером через усреднение (через points_to_draw_quantity времен между проходами через 0)
        scope_find_period(used_scope);

        // Вычисляем минимум и максимум через усреднение то же количество минимумов и максимумов
        scope_find_amplitude(used_scope);
    }

    // Обновляем графику под рендер (даже для первых точек)
    // scope_screens_gui_renew_by_signal_data(used_scope);

}


void scope_update(Scope* used_scope)
{
    // Базовые элементы GUI - Обновляются всегда

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


        // Обновляем графику под рендер (даже для первых точек)
        scope_screens_gui_renew_by_signal_data(used_scope);

        // Заполняемся точками
        signal_render_ctx* sig;

        sig = &used_scope->scope_render_data.signal_render_data;

        build_fixed_time_render(used_scope, sig);

        // Рисуем сигнал
        draw_signal(used_scope, used_scope->scope_render_data.renderer);


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

    // ===== 1. сигнал рендера =====
    scope_screen_gui_delete(used_scope);

    // ===== 2. UI элементы (если у них есть destroy) =====


    // ===== 3. буфер =====
    // НЕ НУЖНО free — он static array внутри struct
    // но можно “обнулить состояние”

    used_scope->signal_control_data.scope_buffer_data.head = 0;
    used_scope->signal_control_data.scope_buffer_data.count = 0;
}

// =========================================================================================== API


// =========================================================================================== HELPER-FUNCTIONS


void zero_crossings_check(Scope* used_scope)
{
    // сравнивает последние 2 сэмпла
    // если найден переход:
    // сохраняет timestamp в отдельный ring buffer

    if (!used_scope) return;

    scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
    scope_buffer_ctx* buffer = &ctrl->scope_buffer_data;
    zero_crossing_ctx* zc = &ctrl->zero_crossings;

    if (buffer->count < 2) return;

    int head = buffer->head;

    // предыдущий индекс (последний записанный сэмпл)
    int prev_index = (head - 1 + BUFFER_SIZE) % BUFFER_SIZE;
    int curr_index = (head - 2 + BUFFER_SIZE) % BUFFER_SIZE;

    float prev_value = buffer->samples[curr_index].value;
    float curr_value = buffer->samples[prev_index].value;


    float treshold = used_scope->signal_control_data.controlled_signal->current_treshold;

    // ===== zero crossing вверх =====
    if (prev_value < -treshold  && curr_value >= treshold)
    {
        double t = buffer->samples[prev_index].time;

        zc->times[zc->head] = t;

        zc->head = (zc->head + 1) % MAX_ZERO_CROSSINGS_TO_CHECK;

        if (zc->count < MAX_ZERO_CROSSINGS_TO_CHECK)
            zc->count++;
    }
}


void scope_signal_buffer_init(Scope* used_scope)
{
    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;

    memset(buffer->samples, 0, sizeof(buffer->samples));

    buffer->head = 0;
    buffer->count = 0;
}


void scope_zero_cross_buffer_init(Scope* used_scope)
{
    zero_crossing_ctx* zc =
        &used_scope->signal_control_data.zero_crossings;

    memset(zc->times, 0, sizeof(zc->times));

    zc->head = 0;
    zc->count = 0;
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


void scope_screen_gui_init(Scope* used_scope)
{
    signal_render_ctx* render = &used_scope->scope_render_data.signal_render_data;

    render->size = RENDER_POINTS_BUFFER_SIZE;

    // ничего не выделяем
    // память уже существует внутри struct
}


void scope_screen_gui_delete(Scope* used_scope)
{
    // ничего не free
    // потому что нет heap-памяти
}



void scope_find_period(Scope* used_scope)
{
    // Берёт zero-crossing события (НЕ сигнал)
    // считает интервалы между ними
    // фильтрует мусор
    // усредняет
    // получает:
    // period
    // frequency

    scope_signal_control_ctx* signal_ctx = &used_scope->signal_control_data;
    zero_crossing_ctx* crossings_ctx = &signal_ctx->zero_crossings;

    // Если слишком мало пересечений — период определить нельзя
    if (crossings_ctx->count < 4) return;

    double measured_periods[MAX_ZERO_CROSSINGS_TO_CHECK];
    int measured_period_count = 0;

    // индекс последнего записанного пересечения
    int latest_crossing_index = crossings_ctx->head;

    // ============================================================
    // Проходим по истории zero-crossing событий (с конца)
    // ============================================================
    for (
        
        int i = 0;
        i < crossings_ctx->count - 1 && measured_period_count < MAX_ZERO_CROSSINGS_TO_CHECK;
        i++
        
    )
    {
        // берём два соседних пересечения назад по времени
        int current_index =
            (latest_crossing_index - i - 1 + MAX_ZERO_CROSSINGS_TO_CHECK) % MAX_ZERO_CROSSINGS_TO_CHECK;

        int previous_index =
            (latest_crossing_index - i - 2 + MAX_ZERO_CROSSINGS_TO_CHECK) % MAX_ZERO_CROSSINGS_TO_CHECK;

        double current_time = crossings_ctx->times[current_index];
        double previous_time = crossings_ctx->times[previous_index];

        // разница времени между двумя последовательными пересечениями
        double time_difference = current_time - previous_time;

        // ========================================================
        // Фильтр мусора:
        // отбрасываем отрицательные и нереалистичные интервалы
        // ========================================================
        if (time_difference > 0.0 && time_difference < 10.0)
        {
            measured_periods[measured_period_count++] = time_difference;
        }
    }

    // если не удалось получить ни одного валидного периода
    if (measured_period_count == 0) return;

    // ============================================================
    // Усреднение всех измеренных периодов
    // ============================================================
    double period_sum = 0.0;

    for (int i = 0; i < measured_period_count; i++)
    {
        period_sum += measured_periods[i];
    }

    double average_period = period_sum / measured_period_count;

    // ============================================================
    // Обновление результата анализа сигнала
    // ============================================================
    signal_ctx->current_period_value = average_period;

    signal_ctx->current_frequency_value = (average_period > 0.0) ? (1.0 / average_period) : 0.0;
}


void scope_find_amplitude(Scope* used_scope)
{
    scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
    scope_buffer_ctx* buffer = &ctrl->scope_buffer_data;

    if (buffer->count < 4) return;
    if (ctrl->current_frequency_value <= 0.0f) return;

    int head = buffer->head;

    // =========================================================
    // 1. определяем окно ~ 3 периода сигнала
    // =========================================================

    double freq = (double)ctrl->current_frequency_value;

    int samples_per_period = (int)(SCOPE_SAMPLE_RATE / freq);
    int window_size = samples_per_period * 3;

    if (window_size < 10)
        window_size = 10;

    if (window_size > buffer->count)
        window_size = buffer->count;

    // =========================================================
    // 2. границы кольцевого окна
    // =========================================================

    int start = (head - window_size + BUFFER_SIZE) % BUFFER_SIZE;
    int end   = (head - 1 + BUFFER_SIZE) % BUFFER_SIZE;

    // =========================================================
    // 3. поиск min/max
    // =========================================================

    float min_value = FLT_MAX;
    float max_value = -FLT_MAX;

    int i = start;

    while (1)
    {
        float v = buffer->samples[i].value;

        if (v < min_value) min_value = v;
        if (v > max_value) max_value = v;

        if (i == end)
            break;

        i = (i + 1) % BUFFER_SIZE;
    }

    ctrl->current_min_signal_value = min_value;
    ctrl->current_max_signal_value = max_value;

    // =========================================================
    // 4. амплитуда
    // =========================================================

    float raw_amplitude = (max_value - min_value) * 0.5f;

    // первый запуск
    if (!ctrl->amplitude_initialized)
    {
        ctrl->amplitude_estimate = raw_amplitude;
        ctrl->amplitude_initialized = true;
    }
    else
    {
        // ОДНО сглаживание (важно: убрали двойное)
        ctrl->amplitude_estimate =
            0.85f * ctrl->amplitude_estimate +
            0.15f * raw_amplitude;
    }

    if (ctrl->amplitude_estimate < 0.001f)
        ctrl->amplitude_estimate = 0.001f;

    // =========================================================
    // 5. обновление threshold (не каждый вызов!)
    // =========================================================

    ctrl->threshold_update_accumulator++;

    const int THRESHOLD_UPDATE_RATE = 10;

    if (ctrl->threshold_update_accumulator >= THRESHOLD_UPDATE_RATE)
    {
        float zc_threshold = ctrl->amplitude_estimate * 0.02f;

        if (zc_threshold < 0.001f)
            zc_threshold = 0.001f;

        ctrl->controlled_signal->current_treshold = zc_threshold;

        ctrl->threshold_update_accumulator = 0;
    }
}


// ===== Утиллиты для scope_screens_gui_renew_by_signal_data =====

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


// TODO: НЕ РАБОТАЕТ
void build_fixed_time_render(Scope* used_scope, signal_render_ctx* render_data)
{
    if (!used_scope || !render_data) return;

    scope_signal_control_ctx* ctrl = &used_scope->signal_control_data;
    scope_buffer_ctx* buffer = &ctrl->scope_buffer_data;
    scope_render_ctx* render_buffer = &used_scope->scope_render_data;

    if (!buffer || !render_buffer) return;
    if (!render_data->points) return;
    if (buffer->count < 2) return;

    const int display_w = render_buffer->gui_parameters.display_w;
    const int display_h = render_buffer->gui_parameters.display_h;

    if (display_w <= 0 || display_h <= 0) return;

    // =========================================================
    // 1. Кол-во точек рендера (CAPACITY SAFE)
    // =========================================================

    int points_to_draw_quantity = display_w * SCOPE_SCREEN_OVERSAMPLING;

    if (points_to_draw_quantity <= 0)
        return;

    if (points_to_draw_quantity > RENDER_POINTS_BUFFER_SIZE)
        points_to_draw_quantity = RENDER_POINTS_BUFFER_SIZE;

    render_data->size = points_to_draw_quantity;

    // =========================================================
    // 2. Временное окно
    // =========================================================

    double time_window =
        used_scope->scope_render_data.gui_parameters.display_width_units *
        used_scope->main_settings.time_val_in_one_unit;

    if (time_window <= 0.0) return;

    int window_samples = (int)(time_window * SCOPE_SAMPLE_RATE);

    if (window_samples < 2)
        window_samples = 2;

    if (window_samples > buffer->count)
        window_samples = buffer->count;

    if (window_samples < 2)
        return;

    // =========================================================
    // 3. START INDEX (SAFE RING BUFFER)
    // =========================================================

    int start_index = buffer->head - window_samples;

    start_index %= BUFFER_SIZE;
    if (start_index < 0)
        start_index += BUFFER_SIZE;

    // =========================================================
    // 4. SCALE (PHYSICAL MODE)
    // =========================================================

    float volts_per_unit = (float)used_scope->main_settings.signal_val_in_one_unit;
    if (volts_per_unit < 1e-6f)
        volts_per_unit = 1.0f;

    float pixels_per_unit = (float)display_h / volts_per_unit;

    int zero_y =
        render_buffer->gui_parameters.display_y +
        display_h / 2;

    // =========================================================
    // 5. FIXED TIME SAMPLING (NO FLOAT ACCUMULATION BUGS)
    // =========================================================

    // ключевой фикс:
    // убираем float acc вообще
    // используем прямую дискретизацию

    for (int i = 0; i < points_to_draw_quantity; i++)
    {
        signal_render_point* p = &render_data->points[i];

        // нормализованный индекс 0..1
        int sample_offset =
            (i * window_samples) / points_to_draw_quantity;

        int idx = start_index + sample_offset;

        idx %= BUFFER_SIZE;
        if (idx < 0)
            idx += BUFFER_SIZE;

        float v = buffer->samples[idx].value;

        // X
        p->x =
            render_buffer->gui_parameters.display_x +
            (i * display_w / points_to_draw_quantity);

        // Y (physical scale)
        float units = v / volts_per_unit;

        p->y = zero_y - (int)(units * pixels_per_unit);

        p->show = true;
    }
}


void build_scroll_render(Scope* used_scope, signal_render_ctx* render_data)
{

}

void build_period_render(Scope* used_scope, signal_render_ctx* render_data)
{

}

/*
void build_period_render(Scope* used_scope, signal_render_ctx* render_data)
{

          
    // =========================================================
    // 5. Поиск min/max в окне (нормализация Y)
    // =========================================================
    // Нам нужно понять диапазон сигнала,
    // чтобы растянуть его на высоту дисплея

    float min_v = FLT_MAX;
    float max_v = -FLT_MAX;

    int scan_idx = buffer_render_start_sample_idx;

    // пробегаем всё окно и ищем границы сигнала
    for (int i = 0; i < window_samples; i++)
    {
        float v = buffer->samples[scan_idx].value;

        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;

        scan_idx = (scan_idx + 1) % BUFFER_SIZE;
    }

    // амплитуда (чтобы избежать деления на 0)
    float amp = max_v - min_v;
    if (amp < 1e-6f) amp = 1e-6f;

    // =========================================================
    // 6. Основной рендер: преобразование в точки экрана
    // =========================================================
    // acc — накопитель шага по сэмплам (float точность лучше int stepping)

    float acc = 0.0f;

    for (int i = 0; i < points_to_draw_quantity; i++)
    {
        signal_render_point* p = &render_data->points[i];

        // =====================================================
        // 6.1. выбираем сэмпл из буфера
        // =====================================================
        // берём старт + смещение по шагу
        int sample_idx = buffer_render_start_sample_idx + (int)acc;

        // защита от выхода за кольцевой буфер
        sample_idx %= BUFFER_SIZE;

        float v = buffer->samples[sample_idx].value;

        // =====================================================
        // 6.2. X координата (равномерное распределение по экрану)
        // =====================================================
        p->x = render_buffer->gui_parameters.display_x +
               (i * display_w / points_to_draw_quantity);

        // =====================================================
        // 6.3. Y нормализация сигнала
        // =====================================================
        // переводим сигнал в диапазон 0..1

        float norm = (v - min_v) / amp;

        // инвертируем Y (SDL экран вниз растёт)
        p->y = render_buffer->gui_parameters.display_y +
               display_h -
               (int)(norm * display_h);

        // точка активна
        p->show = true;

        // =====================================================
        // 6.4. двигаем "виртуальный индекс" по сигналу
        // =====================================================
        acc += step;
    }

}
*/


// ===== Утиллиты для scope_screens_gui_renew_by_signal_data =====


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

    scope_signal_control_ctx* sig = &used_scope->signal_control_data;
    scope_render_ctx* render = &used_scope->scope_render_data;
    scope_main_settings_ctx* ms = &used_scope->main_settings;


    // =========================================================
    // 1. Нормализация значений (UI слой)
    // =========================================================

    const char* freq_unit;
    float freq = format_frequency(sig->current_frequency_value, &freq_unit);

    const char* time_unit;

    float period = format_period(sig->current_period_value, &time_unit);
    format_time(used_scope, &time_unit);

    float time_scale = (float)ms->time_val_in_one_unit;

    const char* volt_unit;
    float max_v = format_voltage(sig->current_max_signal_value, &volt_unit);

    const char* amp_unit;
    float amp = format_voltage(sig->amplitude_estimate, &amp_unit);


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

    float f = used_scope->signal_control_data.current_frequency_value;


    // очистка
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


void scope_buffer_clear(Scope* used_scope)
{
    if (!used_scope) return;

    scope_buffer_ctx* buffer =
        &used_scope->signal_control_data.scope_buffer_data;

    buffer->head = 0;
    buffer->count = 0;

    memset(buffer->samples, 0, sizeof(buffer->samples));

    // ===== сброс derived state =====

    zero_crossing_ctx* zc =
        &used_scope->signal_control_data.zero_crossings;

    zc->head = 0;
    zc->count = 0;
}

// =========================================================================================== HELPER-FUNCTIONS


// =========================================================================================== CALLBACKS

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
        scope_buffer_clear(used_scope);
        used_scope->scope_render_data.scope_on_off_button.pressed_color = used_scope->scope_render_data.main_color_5;


        used_scope->signal_control_data.type_of_controlled_signal = CLEAN_CST;

        used_scope->signal_control_data.controlled_signal->current_treshold = ZC_THRESHOLD_START_VALUE;

        
        used_scope->signal_control_data.threshold_update_accumulator = 0.0f;
        used_scope->signal_control_data.amplitude_estimate = 0;
        used_scope->signal_control_data.amplitude_initialized = false;
    }
}

// =========================================================================================== CALLBACKS

