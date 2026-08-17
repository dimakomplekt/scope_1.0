// scope_gui.c

#define TEST_MODE_6 0 // Новый запуск анализа после on-off (дампы буфферов в коллбеке ON/OFF)
#define TEST_MODE_7 0 // Render волны
#define TEST_MODE_8 0 // Смена режима отображения

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

    Всё, что связано с изображением осциллографа:

        1) геометрия корпуса, дисплеев, сетки и кнопок (scope_gui_renew);
        2) текстбоксы с показаниями (scope_screens_gui_renew_by_signal_data);
        3) подготовка данных развёртки - три режима плюс общее ядро выборки;
        4) собственно отрисовка (draw_signal, scope_render);
        5) коллбеки кнопок.

    Анализ сигнала, буфер и фильтр живут в scope_logic.c.


    КАК УСТРОЕНА ПОДГОТОВКА РАЗВЁРТКИ

    Все три режима сводятся к одному вопросу: "какой кусок времени показываем".
    Отличаются они только тем, как выбирается ЛЕВАЯ ГРАНИЦА окна:

        SCOPE_MODE_FIXED_TIME_STEP_SRM      - окно фиксированной длительности,
                                              привязанное к последнему фронту
        SCOPE_MODE_SCROLL_TO_RIGHT_SRM      - окно, приклеенное к "сейчас",
                                              без всякой привязки (roll)
        SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM - то же, что первый, но длительность
                                              окна равна N измеренным периодам

    Дальше работает общая функция fill_render_points_by_time_window(), которая
    и делает всю тяжёлую работу: выборку по времени, min/max-децимацию,
    интерполяцию, перевод в пиксели и экранный temporal-фильтр.

*/


// =========================================================================================== Утиллиты для форматирования

static float format_frequency(float f, const char** unit)
{
    if (f >= 1e6f) { *unit = "MHz"; return f * 1e-6f; }
    if (f >= 1e3f) { *unit = "kHz"; return f * 1e-3f; }
    if (f >= 1.0f)  { *unit = "Hz";  return f; }


    // Base 
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

// =========================================================================================== Утиллиты для форматирования


// =========================================================================================== TEXTBOX OWNERSHIP HELPERS


/*

    ЗАЧЕМ ЭТА ОБЁРТКА

    Textbox_init() возвращает УКАЗАТЕЛЬ на выделенный malloc-ом текстбокс,
    а поля осциллографа хранят текстбоксы ПО ЗНАЧЕНИЮ. В прошлой версии
    это стыковалось так:

        used_scope->scope_render_data.signal_scale_textbox = *Textbox_init(color, size);

    Здесь две проблемы.

    1) Утечка. Указатель, вернувшийся из Textbox_init(), нигде не сохраняется
       и никогда не освобождается. Двадцать один текстбокс за вызов,
       а scope_gui_renew() вызывался дважды - сорок два потерянных блока
       и сорок два открытых TTF_Font.

    2) Падение на пустом месте. Textbox_init() возвращает NULL, если шрифт
       не открылся - например, папка content/fonts не скопирована рядом
       с exe. Разыменование NULL происходит МГНОВЕННО, ещё до первого кадра,
       без единого сообщения. Внешне это выглядит как "приложение просто
       не запускается".

    Обёртка снимает оба вопроса: копирует содержимое в возвращаемое значение,
    освобождает саму обёртку (шрифт при этом переходит во владение копии)
    и корректно отрабатывает NULL.

*/


/**
 * @brief Создаёт текстбокс по значению, не теряя выделенную Textbox_init память.
 *
 * @param color Цвет текста
 * @param font_size Размер шрифта в пунктах
 *
 * @return Готовый текстбокс по значению; при ошибке загрузки шрифта - обнулённый
 *
 */
static Textbox scope_textbox_create(SDL_Color color, int font_size)
{
    Textbox result;

    memset(&result, 0, sizeof(Textbox));


    if (font_size < 1) font_size = 1;


    Textbox* allocated_textbox = Textbox_init(color, font_size);

    if (!allocated_textbox)
    {
        // Шрифт не найден. Обнулённый текстбокс безопасен: Textbox_update()
        // выйдет по !dirty, Textbox_render() выйдет по !texture
        fprintf(stderr, "Textbox font load failed! Textbox will stay empty\n");

        return result;
    }


    result = *allocated_textbox;

    // Освобождаем только саму обёртку: шрифт и текстура теперь принадлежат копии
    free(allocated_textbox);


    return result;
}


/**
 * @brief Закрывает шрифт и текстуру одного текстбокса, хранимого по значению.
 *
 * Обычный Textbox_destroy() здесь не подходит: он вызывает free() для самой
 * структуры, а наши текстбоксы лежат внутри Scope и в куче не находятся.
 *
 * @param textbox Текстбокс, чьи ресурсы освобождаются
 *
 */
static void scope_textbox_release(Textbox* textbox)
{
    if (!textbox) return;


    if (textbox->texture)
    {
        SDL_DestroyTexture(textbox->texture);
        textbox->texture = NULL;
    }

    if (textbox->font)
    {
        TTF_CloseFont(textbox->font);
        textbox->font = NULL;
    }
}


void scope_gui_destroy(Scope* used_scope)
{
    if (!used_scope) return;


    scope_render_ctx* render = &used_scope->scope_render_data;


    // Отдельные текстбоксы

    scope_textbox_release(&render->scope_signature_textbox);

    scope_textbox_release(&render->signal_scale_textbox);
    scope_textbox_release(&render->time_scale_textbox);

    scope_textbox_release(&render->amplitude_textbox);
    scope_textbox_release(&render->frequency_or_period_textbox);


    // Текстбоксы-пояснения

    scope_textbox_release(&render->change_value_scale_instruction_textbox);
    scope_textbox_release(&render->change_time_scale_instruction_textbox);
    scope_textbox_release(&render->change_frequency_instruction_textbox);
    scope_textbox_release(&render->change_amplitude_instruction_textbox);


    // Подписи кнопок

    scope_textbox_release(&render->decrease_value_scale_button.button_text);
    scope_textbox_release(&render->increase_value_scale_button.button_text);

    scope_textbox_release(&render->decrease_time_scale_button.button_text);
    scope_textbox_release(&render->increase_time_scale_button.button_text);

    scope_textbox_release(&render->decrease_frequency_button.button_text);
    scope_textbox_release(&render->increase_frequency_button.button_text);

    scope_textbox_release(&render->decrease_amplitude_button.button_text);
    scope_textbox_release(&render->increase_amplitude_button.button_text);

    scope_textbox_release(&render->signal_change_button.button_text);
    scope_textbox_release(&render->mode_change_button.button_text);
    scope_textbox_release(&render->controlled_signal_play_button.button_text);
    scope_textbox_release(&render->scope_on_off_button.button_text);
}

// =========================================================================================== TEXTBOX OWNERSHIP HELPERS


// =========================================================================================== GUI INIT and CLEAR

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


    // Базово - 0 по центру
    used_scope->scope_render_data.current_zero_shift = 0.0;

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

    memset(render, 0, sizeof(signal_render_ctx));

    render->size = RENDER_POINTS_BUFFER_SIZE;

}

void scope_screen_gui_clear(Scope* used_scope)
{
    // Repeat init

    signal_render_ctx* render = &used_scope->scope_render_data.signal_render_data;

    memset(render, 0, sizeof(signal_render_ctx));

    // memset обнуляет в том числе persistence_valid и trigger_locked,
    // то есть следующий кадр гарантированно строится с чистого листа
    // и не подмешивает в себя координаты предыдущего сигнала
}

// =========================================================================================== GUI INIT and CLEAR


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
    used_scope->scope_render_data.scope_signature_textbox = scope_textbox_create(used_scope->scope_render_data.main_color_2, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));
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




    anchor_points_ctx* ap = &used_scope->scope_render_data.gui_parameters.screen_anchor_points;



    ap->UL.x = scope_top_border_x_1;
    ap->UL.y = scope_top_border_y_1;

    ap->UC.x = used_scope->scope_render_data.gui_parameters.display_x;
    ap->UC.y = scope_top_border_y_1;

    ap->UR.x = scope_top_border_x_2;
    ap->UR.y = scope_top_border_y_2; 


    ap->CL.x = scope_top_border_x_1;
    ap->CL.y = used_scope->scope_render_data.gui_parameters.display_y;
    
    ap->CC.x = used_scope->scope_render_data.gui_parameters.display_x;
    ap->CC.y = used_scope->scope_render_data.gui_parameters.display_y;

    ap->CR.x = scope_top_border_x_2;
    ap->CR.y = used_scope->scope_render_data.gui_parameters.display_y;


    ap->DL.x = scope_low_border_x_1;
    ap->DL.y = scope_low_border_y_1;
    
    ap->DC.x = used_scope->scope_render_data.gui_parameters.display_x;
    ap->DC.y = scope_low_border_y_2 - scope_low_border_y_1;

    ap->DR.x = scope_low_border_x_2;
    ap->DR.y = scope_low_border_y_2;


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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_value_scale_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.value_scale_set_info_x_1;
    used_scope->scope_render_data.change_value_scale_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.value_scale_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_value_scale_instruction_textbox, "VS");


    // Уменьшение

    used_scope->scope_render_data.decrease_value_scale_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_value_scale_button.button_text.x = used_scope->scope_render_data.decrease_value_scale_button.x;
    used_scope->scope_render_data.decrease_value_scale_button.button_text.y = used_scope->scope_render_data.decrease_value_scale_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_value_scale_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_value_scale_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_time_scale_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.time_scale_set_info_x_1;
    used_scope->scope_render_data.change_time_scale_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.time_scale_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_time_scale_instruction_textbox, "TS");


    // Уменьшение
    used_scope->scope_render_data.decrease_time_scale_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_time_scale_button.button_text.x = used_scope->scope_render_data.decrease_time_scale_button.x;
    used_scope->scope_render_data.decrease_time_scale_button.button_text.y = used_scope->scope_render_data.decrease_time_scale_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_time_scale_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_time_scale_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_amplitude_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.amplitude_set_info_x_1;
    used_scope->scope_render_data.change_amplitude_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.amplitude_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_amplitude_instruction_textbox, "AC");


    // Уменьшение

    used_scope->scope_render_data.decrease_amplitude_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_amplitude_button.button_text.x = used_scope->scope_render_data.decrease_amplitude_button.x;
    used_scope->scope_render_data.decrease_amplitude_button.button_text.y = used_scope->scope_render_data.decrease_amplitude_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_amplitude_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_amplitude_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.change_frequency_instruction_textbox.x = used_scope->scope_render_data.gui_parameters.frequency_set_info_x_1;
    used_scope->scope_render_data.change_frequency_instruction_textbox.y = used_scope->scope_render_data.gui_parameters.frequency_set_info_y_1;

    Textbox_set_content(&used_scope->scope_render_data.change_frequency_instruction_textbox, "FC");


    // Уменьшение

    used_scope->scope_render_data.decrease_frequency_button.user_data = (void*)used_scope;

    used_scope->scope_render_data.decrease_frequency_button.user_data = (void*)used_scope;


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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.decrease_frequency_button.button_text.x = used_scope->scope_render_data.decrease_frequency_button.x;
    used_scope->scope_render_data.decrease_frequency_button.button_text.y = used_scope->scope_render_data.decrease_frequency_button.y;

    Textbox_set_content(&used_scope->scope_render_data.decrease_frequency_button.button_text, "-");


    // Увеличение

    used_scope->scope_render_data.increase_frequency_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 48 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.increase_frequency_button.button_text.x = used_scope->scope_render_data.increase_frequency_button.x;
    used_scope->scope_render_data.increase_frequency_button.button_text.y = used_scope->scope_render_data.increase_frequency_button.y;

    Textbox_set_content(&used_scope->scope_render_data.increase_frequency_button.button_text, "+");


    
    // Смена сигнала


    used_scope->scope_render_data.signal_change_button.user_data = (void*)used_scope;
    
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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.signal_change_button.button_text.x = used_scope->scope_render_data.signal_change_button.x;
    used_scope->scope_render_data.signal_change_button.button_text.y = used_scope->scope_render_data.signal_change_button.y;
        
    Textbox_set_draw_mode(&used_scope->scope_render_data.signal_change_button.button_text, VERTICAL_TEXT);
    Textbox_set_content(&used_scope->scope_render_data.signal_change_button.button_text, "SIGNAL");



    // Смена режима

    used_scope->scope_render_data.mode_change_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.mode_change_button.button_text.x = used_scope->scope_render_data.mode_change_button.x;
    used_scope->scope_render_data.mode_change_button.button_text.y = used_scope->scope_render_data.mode_change_button.y;
        
    Textbox_set_draw_mode(&used_scope->scope_render_data.mode_change_button.button_text, VERTICAL_TEXT);
    Textbox_set_content(&used_scope->scope_render_data.mode_change_button.button_text, "MODE");


    // Проигрывание сигнала

    used_scope->scope_render_data.controlled_signal_play_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.controlled_signal_play_button.button_text.x = used_scope->scope_render_data.controlled_signal_play_button.x;
    used_scope->scope_render_data.controlled_signal_play_button.button_text.y = used_scope->scope_render_data.controlled_signal_play_button.y;
        
    Textbox_set_draw_mode(&used_scope->scope_render_data.controlled_signal_play_button.button_text, VERTICAL_TEXT);
    Textbox_set_content(&used_scope->scope_render_data.controlled_signal_play_button.button_text, "PLAY");


    // Включение-выключение осциллографа 
    used_scope->scope_render_data.scope_on_off_button.user_data = (void*)used_scope;

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
        scope_textbox_create(used_scope->scope_render_data.main_color_1, 24 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.gui_parameters.signal_scale_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.gui_parameters.time_scale_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.gui_parameters.amplitude_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

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
        scope_textbox_create(used_scope->scope_render_data.gui_parameters.frequency_or_period_info_fill_color_1, 30 / (50 / used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit));

    used_scope->scope_render_data.frequency_or_period_textbox.x = used_scope->scope_render_data.gui_parameters.frequency_or_period_info_x_1;
    used_scope->scope_render_data.frequency_or_period_textbox.y = used_scope->scope_render_data.gui_parameters.frequency_or_period_info_y_1;

    // Init значение текста - поменяется сразу при обновлении буфера
    Textbox_set_content(&used_scope->scope_render_data.frequency_or_period_textbox, "FREQUENCY");

    // FIX: снимаем флаг перестройки GUI.
    //
    // Раньше флаг не сбрасывался никогда, а scope_gui_renew() вызывался дважды
    // (из scope_gui_init() и повторно из scope_init()). Каждый вызов заново
    // создавал 21 текстбокс через Textbox_init(), то есть открывал 21 TTF_Font
    // и терял 21 malloc-блок. Теперь перестройка выполняется ровно один раз
    // на один выставленный флаг.

    used_scope->scope_render_data.scope_gui_need_update = false;
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



    if (signal->measured_signal_characteristics.measured_max == -FLT_MAX ||
    signal->measured_signal_characteristics.measured_min == FLT_MAX) 
    
        return;


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

    float step_v = format_voltage(used_scope->main_settings.signal_val_in_one_unit, &volt_unit);

    const char* amp_unit;
    float amp = format_voltage(signal->measured_signal_characteristics.measured_amplitude, &amp_unit);


    // =========================================================
    // 2. Текстбоксы (UI слой)
    // =========================================================

    // Единицы
    if (ms->current_signal_units == VOLTS_SU)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "SV: %.3f %s", step_v, volt_unit);
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
}


void main_screen_renew(Scope* used_scope)
{
    // ===== Присвоение данных о сигнале для рендера ====
    scope_signal_control_ctx* signal = &used_scope->signal_control_data;

    if (signal->scope_buffer_data.count == 0) return;


    signal_render_ctx* signal_render = &used_scope->scope_render_data.signal_render_data;


    if (TEST_MODE_7)
    {
        printf("\n\nЗона рассчёта!\n\n");
    }

    switch (used_scope->main_settings.current_mode)
    {
        case SCOPE_MODE_FIXED_TIME_STEP_SRM:

            if (TEST_MODE_7)
            {
                printf("\n\nЗашли в выбор тайм расчёта!\n\n");
            }

            build_fixed_time_render(used_scope, signal_render);
            break;


        case SCOPE_MODE_SCROLL_TO_RIGHT_SRM:

            build_scroll_render(used_scope, signal_render);   
            break;


        case SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM:
        
            build_fixed_period_render(used_scope, signal_render);
            break;
        

        default:
            break;

    }
}


// =========================================================================================== SIGNAL SWEEP BUILDING


/*

    ПОЧЕМУ РАЗВЁРТКА ПЕРЕПИСАНА ЦЕЛИКОМ

    Старая версия build_fixed_time_render() (и её копия build_fixed_period_render())
    страдала сразу пятью независимыми болезнями. Разберём по порядку, потому
    что каждая из них давала свой вклад в дёрганье, а две из них ещё и роняли
    приложение.

    1. НЕОГРАНИЧЕННЫЙ ПОИСК ЛЕВОЙ ГРАНИЦЫ - ПАДЕНИЕ

        int window_start_idx = newest_idx;

        while (!history_finished && !enough_history)
        {
            window_start_idx--;
            unsigned int curr_idx = (window_start_idx + BUFFER_SIZE) % BUFFER_SIZE;
            ...
        }

       Счётчик уменьшался без нижней границы. Выход из цикла зависел только
       от того, найдётся ли пересечение нуля - а на свежезаполненном нулями
       буфере (например, сразу после выключения-включения) его нет вообще.
       Цикл проходил кольцо насквозь и уезжал в отрицательные значения;
       как только window_start_idx становился меньше -BUFFER_SIZE, выражение
       (window_start_idx + BUFFER_SIZE) % BUFFER_SIZE давало ОТРИЦАТЕЛЬНЫЙ
       результат, и дальше шло чтение samples[-N] - обращение до начала
       девятимегабайтного массива. Иногда это молча читало чужую память
       (программа жила дальше и вела себя странно), иногда попадало
       в незамапленную страницу и падало, иногда цикл просто крутился
       вечно и приложение "зависало". Все три симптома из описания задачи.

    2. РАЗВЁРТКА ЗНАЧЕНИЯ ИНДЕКСА В unsigned - ЕЩЁ ОДНО ПАДЕНИЕ

        unsigned int render_start_idx = window_start_idx;   // window_start_idx мог быть < 0

       Отрицательный int, приведённый к unsigned, превращается в число порядка
       четырёх миллиардов. Следующая же строка buffer->samples[render_start_idx]
       читала память за гигабайты от массива.

    3. НЕОГРАНИЧЕННЫЙ ПОИСК СЭМПЛА ПОД ПИКСЕЛЬ - ЗАВИСАНИЕ

        while (buffer->samples[next_s_index].time < target_time)
            next_s_index = (next_s_index + BUFFER_SIZE + 1) % BUFFER_SIZE;

       Если target_time оказывалось больше любого времени в буфере (а после
       пунктов 1-2 оно легко оказывалось мусорным), цикл наматывал кольцо
       бесконечно.

    4. ТРИГГЕР БЕЗ ГИСТЕРЕЗИСА И БЕЗ ИНТЕРПОЛЯЦИИ - ГЛАВНАЯ ПРИЧИНА ДЁРГАНЬЯ

       Точка привязки искалась как "сэмпл, у которого сосед справа ниже
       dc_offset, а сам он выше". На шумном сигнале таких точек за период
       набираются десятки, и каждый кадр выбиралась другая. Плюс привязка
       шла к целому сэмплу, а не к моменту реального пересечения: при 800
       пикселях на 16 мс один сэмпл - это почти целый пиксель, и фаза
       кадр к кадру гуляла на этот пиксель.

    5. ВЫБОРКА "ПЕРВЫЙ СЭМПЛ ПОЗЖЕ ЦЕЛЕВОГО ВРЕМЕНИ" - АЛИАСИНГ

       Значение пикселя бралось как полусумма двух соседних сэмплов около
       найденного индекса. Если на пиксель приходится сто сэмплов - берутся
       два случайных из ста, и картинка каждый кадр разная, хотя сигнал
       не менялся.


    ЧТО СДЕЛАНО ВМЕСТО

    Развёртка приведена к схеме, по которой работают настоящие цифровые
    осциллографы (Rigol DS1000Z, Siglent SDS1000X и любой другой DSO):

        TRIGGER -> WINDOW -> DECIMATION -> PERSISTENCE

    TRIGGER (find_trigger_time)

        Честный edge-триггер с гистерезисом: фронт засчитывается, только если
        сигнал ДО него реально был ниже level - hysteresis, а ПОСЛЕ реально
        поднялся выше level + hysteresis. Момент пересечения уточняется
        линейной интерполяцией и получается ДРОБНЫМ - это убирает джиттер
        в пределах одного сэмпла. Поиск идёт назад от самой свежей точки,
        то есть окно всегда прибито к последнему фронту, а не к случайному.

    WINDOW

        Левая граница = момент триггера минус pre-trigger часть экрана.
        Всё дальнейшее работает с ВРЕМЕНЕМ, а не с индексами, поэтому
        уехать за границы массива уже невозможно в принципе.

    DECIMATION (fill_render_points_by_time_window)

        Для каждого пикселя собираются ВСЕ сэмплы, попавшие в его временной
        интервал, и запоминаются минимум с максимумом. Рисуется вертикальный
        штрих от min до max. Если сэмплов в пикселе меньше одного - значение
        берётся линейной интерполяцией между соседями.

    PERSISTENCE

        Экранная координата пропускается через фильтр первого порядка
        относительно предыдущего кадра. Остаточное дрожание на доли пикселя,
        которое неизбежно возникает при округлении до целых координат,
        размазывается по трём кадрам и глазом не ловится.

*/


/**
 * @brief Ищет индекс сэмпла, ближайшего снизу к заданному моменту времени.
 *
 * Идёт назад от самой свежей точки, но строго не дальше, чем реально
 * накоплено данных. Именно отсутствие этого ограничения в прошлой версии
 * уводило индекс в отрицательную область.
 *
 * @param buffer Основной кольцевой буфер осциллографа
 * @param target_time Искомый момент времени в секундах
 *
 * @return Индекс сэмпла или -1, если такого времени в буфере уже нет
 *
 */
static int find_sample_index_by_time(scope_buffer_ctx* buffer, double target_time)
{
    if (buffer->count < 2) return -1;


    int newest_idx = (buffer->head + BUFFER_SIZE - 1) % BUFFER_SIZE;

    int max_steps = buffer->count - 1;


    for (int step = 0; step < max_steps; step++)
    {
        int idx = (newest_idx - step + BUFFER_SIZE) % BUFFER_SIZE;

        if (buffer->samples[idx].time <= target_time) return idx;
    }


    return -1;
}


/**
 * @brief Ищет момент начала последней валидной восходящей полуволны.
 *
 * Уровень полуволны определяется текущим running_dc_offset.
 * Для подтверждения начала полуволны используется существующий
 * hysteresis-механизм триггера.
 *
 * В отличие от обычного trigger_time функция возвращает не момент
 * пересечения running_dc_offset, а момент, соответствующий началу
 * восходящей полуволны.
 *
 * Поэтому последующий renderer начинает построение с начала полуволны,
 * а не с произвольного места внутри неё.
 *
 * @param used_scope Осциллограф, чей буфер анализируется
 * @param window_time Полная длительность окна развёртки в секундах
 * @param out_trigger_time Приёмник времени начала полуволны
 *
 * @return true, если валидное начало полуволны найдено
 */
static bool find_trigger_time(
    Scope* used_scope,
    double window_time,
    double* out_trigger_time
)
{
    scope_buffer_ctx* buffer =
        &used_scope->signal_control_data.scope_buffer_data;

    scope_signal_control_ctx* signal =
        &used_scope->signal_control_data;


    // ===== Санитарные проверки =====

    if (buffer->count < 8)
        return false;

    if (!(window_time > 0.0) ||
        !isfinite(window_time))
        return false;

    // ===== Санитарные проверки =====


    // ===== Параметры сигнала =====

    float level =
        signal->running_signal_characteristics.running_dc_offset;

    float hysteresis =
        signal->filter_ctx.running_treshold;


    if (!isfinite(level) ||
        !isfinite(hysteresis) ||
        hysteresis <= 0.0f)
    {
        return false;
    }

    // ===== Параметры сигнала =====


    int newest_idx =
        (buffer->head + BUFFER_SIZE - 1) % BUFFER_SIZE;

    double newest_time =
        buffer->samples[newest_idx].time;


    // ===== Допустимое окно триггера =====
    //
    // Триггер должен оставить достаточно данных:
    //
    // слева  -> pretrigger
    // справа -> остаток экрана
    //
    // Поэтому ищем только внутри допустимого диапазона.


    /*

    double latest_allowed_time =
        newest_time -
        (1.0 - RENDER_PRETRIGGER_PART) *
        window_time;

    */

    double latest_allowed_time =
        newest_time -
        window_time;


    int oldest_idx =
        (buffer->head - buffer->count + BUFFER_SIZE) %
        BUFFER_SIZE;
    
    double oldest_time =
        buffer->samples[oldest_idx].time;
    
    double oldest_allowed_time =
        oldest_time +
        RENDER_PRETRIGGER_PART * window_time;

    if (latest_allowed_time <= oldest_allowed_time)
        return false;

    // ===== Допустимое окно триггера =====


    // ===== Размер зоны проверки гистерезиса =====

    int hysteresis_samples =
        MIN_TRIGGER_HYSTERESIS_SAMPLES;


    float measured_period =
        signal->measured_signal_characteristics.measured_period;


    if (measured_period > 0.0f &&
        isfinite(measured_period))
    {
        hysteresis_samples =
            (int)(
                0.25f *
                measured_period *
                (float)SCOPE_SAMPLE_RATE
            );
    }


    if (hysteresis_samples <
        MIN_TRIGGER_HYSTERESIS_SAMPLES)
    {
        hysteresis_samples =
            MIN_TRIGGER_HYSTERESIS_SAMPLES;
    }


    if (hysteresis_samples >
        MAX_TRIGGER_HYSTERESIS_SAMPLES)
    {
        hysteresis_samples =
            MAX_TRIGGER_HYSTERESIS_SAMPLES;
    }

    // ===== Размер зоны проверки гистерезиса =====


    int max_steps =
        buffer->count - 2;


    // ===== Поиск последнего восходящего перехода =====

    for (int step = 1;
         step < max_steps;
         step++)
    {
        int idx =
            (newest_idx -
             step +
             BUFFER_SIZE) %
            BUFFER_SIZE;


        int next_idx =
            (idx + 1) % BUFFER_SIZE;


        double sample_time =
            buffer->samples[idx].time;



        float value =
            buffer->samples[idx].value;

        float next_value =
            buffer->samples[next_idx].value;


        // ============================================================
        // Кандидат на начало восходящей полуволны
        // ============================================================
        //
        // Здесь по-прежнему находим переход через running_dc_offset.
        // Но сам этот crossing больше НЕ будет результатом функции.
        //
        // Он используется только как опорная точка, относительно
        // которой подтверждаем, что это действительно начало
        // полуволны, а не случайное пересечение шума.
        //

        if (!(value <= level &&
              next_value > level))
        {
            continue;
        }


        // ===== Проверка гистерезиса =====

        bool went_low = false;
        bool went_high = false;


        // ------------------------------------------------------------
        // Назад:
        //
        // До начала фронта сигнал должен действительно находиться
        // ниже нижней границы гистерезиса.
        // ------------------------------------------------------------

        for (int back = 1;
             back <= hysteresis_samples;
             back++)
        {
            int probe_idx =
                (idx -
                 back +
                 BUFFER_SIZE) %
                BUFFER_SIZE;


            float probe_value =
                buffer->samples[probe_idx].value;


            if (probe_value <
                level - hysteresis)
            {
                went_low = true;
                break;
            }


            // Слева сигнал уже находился выше верхней
            // границы гистерезиса.
            //
            // Значит это не начало нового фронта.

            if (probe_value >
                level + hysteresis)
            {
                break;
            }
        }

        // ------------------------------------------------------------
        // Вперёд:
        //
        // После перехода сигнал должен действительно уйти
        // выше верхней границы гистерезиса.
        // ------------------------------------------------------------

        for (int forward = 1;
             forward <= hysteresis_samples;
             forward++)
        {
            int probe_idx =
                (next_idx +
                 forward) %
                BUFFER_SIZE;


            // Дальше свежайшего сэмпла данных нет.
            if (probe_idx ==
                (newest_idx + 1) % BUFFER_SIZE)
            {
                break;
            }


            float probe_value =
                buffer->samples[probe_idx].value;


            if (probe_value >
                level + hysteresis)
            {
                went_high = true;
                break;
            }


            if (probe_value <
                level - hysteresis)
            {
                break;
            }
        }


        if (!went_low ||
            !went_high)
        {
            continue;
        }

        // ===== Проверка гистерезиса =====


        // ============================================================
        // Теперь нашли подтверждённый восходящий фронт.
        // Находим реальное начало полуволны.
        // ============================================================

        double start_time = sample_time;

        float start_level = level - hysteresis;

        bool start_found = false;


        // Идём назад от найденного DC-crossing,
        // пока не найдём пересечение нижней границы.

        for (int back = 1;
            back <= hysteresis_samples;
            back++)
        {
            int current_idx =
                (idx - back + BUFFER_SIZE) % BUFFER_SIZE;

            int next_start_idx =
                (current_idx + 1) % BUFFER_SIZE;

            float current_value =
                buffer->samples[current_idx].value;

            float next_start_value =
                buffer->samples[next_start_idx].value;


            if (current_value <= start_level &&
                next_start_value > start_level)
            {
                double time_1 =
                    buffer->samples[current_idx].time;

                double time_2 =
                    buffer->samples[next_start_idx].time;


                if (next_start_value != current_value)
                {
                    double part =
                        (start_level - current_value) /
                        (next_start_value - current_value);

                    if (part < 0.0)
                        part = 0.0;

                    if (part > 1.0)
                        part = 1.0;

                    start_time =
                        time_1 +
                        part * (time_2 - time_1);
                }
                else
                {
                    start_time = time_1;
                }

                start_found = true;
                break;
            }
        }


        // Не нашли границу начала полуволны.
        // Такой кандидат использовать нельзя.

        if (!start_found)
            continue;


        // ============================================================
        // Теперь проверяем уже НАСТОЯЩИЙ якорь развёртки.
        //
        // Именно start_time будет передан renderer,
        // поэтому именно он должен находиться
        // в допустимом временном диапазоне.
        // ============================================================

        if (start_time > latest_allowed_time)
            continue;

        if (start_time < oldest_allowed_time)
            break;


        // ============================================================
        // Валидное начало полуволны найдено.
        // ============================================================

        *out_trigger_time = start_time;

        return true;
    }

    // ===== Поиск последнего восходящего перехода =====


    return false;
}



/**
 * @brief Заполняет буфер точек рендера сигналом из заданного временного окна.
 *
 * Общее ядро всех трёх режимов развёртки. Работает исключительно со временем,
 * поэтому выйти за границы кольцевого буфера здесь невозможно.
 *
 * Делает три вещи:
 *
 *  1. min/max-децимацию, когда в пиксель попадает больше одного сэмпла;
 *  2. линейную интерполяцию, когда в пиксель не попадает ни одного;
 *  3. субпиксельный экранный temporal-фильтр относительно предыдущего кадра.
 *
 * Temporal-фильтр работает во float-координатах. Это позволяет избежать
 * накопления ошибки от целочисленного округления на каждом кадре.
 *
 * Дополнительно применяется защитная зона дисплея:
 *
 *  - слева и справа сигнал не рисуется в области рамки;
 *  - сверху и снизу сигнал не рисуется за внутренними границами дисплея.
 *
 * @param used_scope Осциллограф-владелец данных
 * @param render_data Приёмник точек развёртки
 * @param window_start_time Время левой границы окна в секундах
 * @param pixel_time Сколько времени приходится на один пиксель по горизонтали
 * @param pixel_signal Сколько вольт приходится на один пиксель по вертикали
 * @param points_quantity Сколько точек нужно заполнить (ширина дисплея в пикселях)
 */
static void fill_render_points_by_time_window(

    Scope* used_scope,
    signal_render_ctx* render_data,

    double window_start_time,

    double pixel_time,
    double pixel_signal,

    int points_quantity

)
{
    scope_buffer_ctx* buffer =
        &used_scope->signal_control_data.scope_buffer_data;

    scope_gui_basic_parameters* gui_parameters =
        &used_scope->scope_render_data.gui_parameters;

    anchor_points_ctx* display_anchors =
        &gui_parameters->screen_anchor_points;


    // ===== Санитарные проверки =====

    if (points_quantity < 2)
        return;

    if (points_quantity > RENDER_POINTS_BUFFER_SIZE)
        points_quantity = RENDER_POINTS_BUFFER_SIZE;

    if (!(pixel_time > 0.0) ||
        !(pixel_signal > 0.0))
        return;

    if (!isfinite(window_start_time))
        return;

    // ===== Санитарные проверки =====


    // ===== Опорные величины =====

    double current_zero_shift =
        used_scope->scope_render_data.current_zero_shift;

    int x_0_pixel =
        display_anchors->CL.x;

    int y_0_pixel =
        display_anchors->CL.y +
        (int)(-current_zero_shift / pixel_signal);

    int display_center_y =
        display_anchors->CC.y;


    // Половина высоты дисплея за вычетом защитной зоны рамки.
    //
    // Сигнал не должен доходить до самой линии рамки.
    int half_height_pixels =
        gui_parameters->display_h / 2 -
        gui_parameters->lines_thickness * 2;

    if (half_height_pixels < 1)
        half_height_pixels = 1;


    int top_limit =
        display_center_y - half_height_pixels;

    int bottom_limit =
        display_center_y + half_height_pixels;


    // ===== Защитные зоны по горизонтали =====
    //
    // Оставляем несколько пикселей возле левой и правой рамки.
    //
    // В старой реализации использовалась зона:
    //
    //     lines_thickness * 2
    //
    // Поэтому сохраняем ту же геометрию.

    int left_guard =
        gui_parameters->lines_thickness * 2;

    int right_guard =
        gui_parameters->lines_thickness * 2;

    // ===== Защитные зоны по горизонтали =====


    int newest_idx =
        (buffer->head + BUFFER_SIZE - 1) % BUFFER_SIZE;

    // ===== Опорные величины =====


    int idx =
        find_sample_index_by_time(buffer, window_start_time);


    if (idx < 0)
    {
        // Запрошенного времени в буфере уже нет.
        //
        // Гасим кадр целиком и сбрасываем persistence,
        // чтобы следующий валидный кадр строился с нуля.

        render_data->size = points_quantity;

        for (int i = 0; i < points_quantity; i++)
        {
            render_data->points[i].show = false;
            render_data->points[i].persistence_y = 0.0f;
        }

        render_data->persistence_valid = false;

        return;
    }


    // Temporal-фильтр применим только если предыдущий кадр
    // имел ту же геометрию по горизонтали.
    bool persistence_ready =
        render_data->persistence_valid &&
        (render_data->size == points_quantity);


    bool data_exhausted = false;


    for (int i = 0; i < points_quantity; i++)
    {
        double pixel_start_time =
            window_start_time +
            (double)i * pixel_time;

        double pixel_end_time =
            pixel_start_time +
            pixel_time;


        // ===== Сбор всех сэмплов, попавших в текущий пиксель =====

        float value_min = FLT_MAX;
        float value_max = -FLT_MAX;

        int samples_in_pixel = 0;


        while (!data_exhausted &&
               buffer->samples[idx].time < pixel_end_time)
        {
            float value =
                buffer->samples[idx].value;


            if (value < value_min)
                value_min = value;

            if (value > value_max)
                value_max = value;


            samples_in_pixel += 1;


            if (idx == newest_idx)
            {
                data_exhausted = true;
                break;
            }


            idx =
                (idx + 1) % BUFFER_SIZE;
        }

        // ===== Сбор всех сэмплов, попавших в текущий пиксель =====


        float value_center;


        if (samples_in_pixel > 0)
        {
            value_center =
                0.5f * (value_min + value_max);
        }
        else if (!data_exhausted)
        {
            // На быстрых развёртках на пиксель приходится
            // меньше одного сэмпла.
            //
            // Берём линейную интерполяцию между соседними
            // сэмплами.

            int prev_idx =
                (idx - 1 + BUFFER_SIZE) % BUFFER_SIZE;


            double time_1 =
                buffer->samples[prev_idx].time;

            double time_2 =
                buffer->samples[idx].time;


            float value_1 =
                buffer->samples[prev_idx].value;

            float value_2 =
                buffer->samples[idx].value;


            double span =
                time_2 - time_1;


            if (span > 0.0)
            {
                double part =
                    (pixel_start_time - time_1) / span;


                if (part < 0.0)
                    part = 0.0;

                if (part > 1.0)
                    part = 1.0;


                value_center =
                    value_1 +
                    (float)part *
                    (value_2 - value_1);
            }
            else
            {
                value_center = value_2;
            }


            value_min = value_center;
            value_max = value_center;
        }
        else
        {
            // Данные закончились.
            // Остаток кадра не показываем.

            render_data->points[i].x =
                x_0_pixel + i;

            render_data->points[i].show = false;

            continue;
        }


        // ===== Перевод в экранные координаты =====

        float y_center_float =
            (float)y_0_pixel -
            value_center / (float)pixel_signal;

        float y_top_float =
            (float)y_0_pixel -
            value_max / (float)pixel_signal;

        float y_bottom_float =
            (float)y_0_pixel -
            value_min / (float)pixel_signal;

        // ===== Перевод в экранные координаты =====


        // ===== Экранный temporal-фильтр =====

        float filtered_y_float =
            y_center_float;


        if (persistence_ready)
        {
            float previous_y =
                render_data->points[i].persistence_y;


            float delta =
                y_center_float - previous_y;

            float abs_delta =
                fabsf(delta);


            float beta;


            if (abs_delta < 1.0f)
            {
                beta = 0.20f;
            }
            else if (abs_delta < 3.0f)
            {
                beta = 0.35f;
            }
            else if (abs_delta < 6.0f)
            {
                beta = 0.55f;
            }
            else
            {
                beta = 0.85f;
            }


            filtered_y_float =
                previous_y +
                beta * delta;
        }


        render_data->points[i].persistence_y =
            filtered_y_float;


        float correction =
            filtered_y_float - y_center_float;


        float filtered_y_top =
            y_top_float + correction;

        float filtered_y_bottom =
            y_bottom_float + correction;


        // ===== Перевод в целые пиксели =====

        int y_center =
            (int)lroundf(filtered_y_float);

        int y_top =
            (int)lroundf(filtered_y_top);

        int y_bottom =
            (int)lroundf(filtered_y_bottom);

        // ===== Перевод в целые пиксели =====


        // ===== Защита границ дисплея =====

        // Горизонтальная защитная зона.
        bool inside_horizontal =
            (i >= left_guard) &&
            (i < points_quantity - right_guard);


        // Вертикальная видимость.
        //
        // До клиппинга проверяем, пересекает ли исходный
        // min/max-отрезок рабочую область вообще.

        bool inside_vertical =
            (y_bottom <= bottom_limit) &&
            (y_top >= top_limit);


        bool inside =
            inside_horizontal &&
            inside_vertical;


        // Полный клиппинг min/max-отрезка.
        //
        // Теперь ни одна координата диапазона не может
        // физически оказаться за пределами рабочей области.

        if (y_top < top_limit)
            y_top = top_limit;

        if (y_top > bottom_limit)
            y_top = bottom_limit;

        if (y_bottom < top_limit)
            y_bottom = top_limit;

        if (y_bottom > bottom_limit)
            y_bottom = bottom_limit;


        // Центр тоже ограничиваем рабочей областью.
        // Это особенно важно для случая, когда весь сигнал
        // или значительная его часть ушла за верх/низ.

        if (y_center < top_limit)
            y_center = top_limit;

        if (y_center > bottom_limit)
            y_center = bottom_limit;

        // ===== Защита границ дисплея =====


        render_data->points[i].x =
            x_0_pixel + i;

        render_data->points[i].y =
            y_center;

        render_data->points[i].y_max =
            y_top;

        render_data->points[i].y_min =
            y_bottom;

        render_data->points[i].show =
            inside;
    }


    render_data->size =
        points_quantity;

    render_data->persistence_valid =
        true;
}


/**
 * @brief Считает, сколько вольт приходится на один пиксель по вертикали.
 *
 * Вынесено отдельно, потому что вертикальный масштаб одинаков во всех трёх
 * режимах развёртки и отличается только горизонтальный.
 *
 * @param used_scope Осциллограф, чьи настройки читаются
 *
 * @return Цена одного пикселя в вольтах (всегда больше нуля)
 *
 */
static double get_pixel_signal_scale(Scope* used_scope)
{
    scope_main_settings_ctx* settings = &used_scope->main_settings;

    scope_render_ctx* render_parameters = &used_scope->scope_render_data;
    scope_gui_basic_parameters* gui_parameters = &render_parameters->gui_parameters;


    int height_pixels =
        gui_parameters->display_height_units *
        render_parameters->basic_pixels_quantity_in_equivalent_unit;

    if (height_pixels < 1) height_pixels = 1;


    int signal_scale = settings->signal_val_in_one_unit;

    if (signal_scale < 1) signal_scale = 1;


    // Инициализация обязательна: в прошлой версии переменная объявлялась
    // без значения, а ветка default в switch ничего ей не присваивала -
    // при любом неожиданном значении енума в расчёт уходил мусор
    double signal_unit_multiplier = 1.0;

    switch (settings->current_signal_units)
    {
        case VOLTS_SU:
            signal_unit_multiplier = 1.0;
            break;

        default:
            signal_unit_multiplier = 1.0;
            break;
    }


    double whole_screen_signal =
        gui_parameters->display_height_units *
        signal_scale *
        signal_unit_multiplier;


    return whole_screen_signal / (double)height_pixels;
}


/**
 * @brief Считает полную длительность экрана для режимов с фиксированным временем.
 *
 * @param used_scope Осциллограф, чьи настройки читаются
 *
 * @return Время всего экрана в секундах
 *
 */
static double get_whole_screen_time(Scope* used_scope)
{
    scope_main_settings_ctx* settings = &used_scope->main_settings;

    scope_gui_basic_parameters* gui_parameters = &used_scope->scope_render_data.gui_parameters;


    int time_scale = settings->time_val_in_one_unit;

    if (time_scale < 1) time_scale = 1;


    // Тот же случай неинициализированной переменной, что и выше
    double time_unit_multiplier = 1e-3;

    switch (settings->current_time_units)
    {
        case MICROSECONDS_TU:
            time_unit_multiplier = 1e-6;
            break;

        case MILLISECONDS_TU:
            time_unit_multiplier = 1e-3;
            break;

        case SECONDS_TU:
            time_unit_multiplier = 1.0;
            break;

        default:
            time_unit_multiplier = 1e-3;
            break;
    }


    return (double)gui_parameters->display_width_units * (double)time_scale * time_unit_multiplier;
}



/**
 * @brief Общая часть триггерных режимов: найти окно и заполнить точки.
 *
 * Отличие fixed-time от fixed-period сводится ровно к одному числу -
 * длительности экрана. Всё остальное совпадает построчно, поэтому вынесено
 * сюда, а не продублировано в двух местах, как было раньше.
 *
 * @param used_scope Осциллограф-владелец данных
 * @param render_data Приёмник точек развёртки
 * @param whole_screen_time Полная длительность окна в секундах
 *
 */
static void build_triggered_render(

    Scope* used_scope,
    signal_render_ctx* render_data,
    double whole_screen_time

)
{
    scope_render_ctx* render_parameters = &used_scope->scope_render_data;
    scope_gui_basic_parameters* gui_parameters = &render_parameters->gui_parameters;

    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;


    int width_pixels =
        gui_parameters->display_width_units *
        render_parameters->basic_pixels_quantity_in_equivalent_unit;


    if (!(whole_screen_time > 0.0) || !isfinite(whole_screen_time)) return;

    if (buffer->count < 8) return;


    double pixel_time = whole_screen_time / (double)width_pixels;

    double pixel_signal = get_pixel_signal_scale(used_scope);


    double trigger_time;

    if (find_trigger_time(used_scope, whole_screen_time, &trigger_time))
    {
        double window_start_time = trigger_time; // - RENDER_PRETRIGGER_PART * whole_screen_time;

        fill_render_points_by_time_window(

            used_scope,
            render_data,
            window_start_time,
            pixel_time,
            pixel_signal,
            width_pixels

        );

        render_data->trigger_locked = true;
        render_data->last_trigger_time = trigger_time;

        return;
    }



    // ===== AUTO-режим =====
    //
    // Триггера нет: сигнал апериодический, данных ещё мало или волна
    // не выходит за зону гистерезиса. Настоящие осциллографы в этой
    // ситуации не гасят экран, а показывают свежий кусок без привязки -
    // делаем так же

    int newest_idx = (buffer->head + BUFFER_SIZE - 1) % BUFFER_SIZE;

    double newest_time = buffer->samples[newest_idx].time;


    fill_render_points_by_time_window(

        used_scope,
        render_data,
        newest_time - whole_screen_time,
        pixel_time,
        pixel_signal,
        width_pixels

    );

    render_data->trigger_locked = false;
}


void build_fixed_time_render(Scope* used_scope, signal_render_ctx* render_data)
{
    if (TEST_MODE_7)
    {
        printf("\n\nНачинаем строить рендер-контекст для фиксированного времени\n\n");
    }


    // Развёртка с фиксированным временем на деление.
    //
    // Длительность экрана целиком задаётся настройками пользователя:
    // сколько единиц времени приходится на одно деление сетки, умножить
    // на количество делений по горизонтали

    build_triggered_render(used_scope, render_data, get_whole_screen_time(used_scope));


    if (TEST_MODE_7)
    {
        printf("\n\nПостроили контекст для фиксированного времени! trigger=%d\n\n", render_data->trigger_locked);
    }
}


void build_scroll_render(Scope* used_scope, signal_render_ctx* render_data)
{
    if (TEST_MODE_7)
    {
        printf("\n\nНачинаем строить рендер-контекст для roll-режима\n\n");
    }


    /*

        ROLL-РЕЖИМ (САМОПИСЕЦ)

        Концепция.

            Триггера нет вообще. Правый край экрана - это всегда "сейчас",
            картинка непрерывно уезжает влево, как лента самописца или
            как график загрузки процессора в диспетчере задач.

        Зачем он нужен, если есть два других режима.

            Триггерная развёртка показывает СТОЯЧУЮ картинку: каждый кадр
            заново выравнивается по фронту, и волна как будто застыла.
            Это идеально для периодического сигнала и совершенно бесполезно
            для медленного апериодического: если сигнал меняется раз
            в несколько секунд, триггеру не за что зацепиться, а даже
            зацепившись, он покажет всё тот же кусок.

            Roll показывает ИСТОРИЮ. Видно, как параметр менялся последние
            N секунд, видно одиночные события, видно дрейф. Реальные
            осциллографы включают этот режим автоматически на развёртках
            медленнее примерно 50 мс на деление.

        Как работает.

            Левая граница окна = время самого свежего сэмпла минус полная
            длительность экрана. Всё. Никакого поиска фронта, никакого
            holdoff. Дальше отрабатывает то же самое общее ядро выборки,
            что и в триггерных режимах.

        Почему картинка при этом НЕ дёргается.

            Потому что дёрганье в триггерных режимах берётся именно
            из перевыбора точки привязки. Здесь точка привязки одна
            и монотонно растёт вместе со временем, поэтому кадр от кадра
            смещается ровно на (время между кадрами / pixel_time) пикселей -
            то есть картинка едет с постоянной скоростью.

        Когда режим бессмысленен.

            Когда сдвиг за кадр становится больше нескольких пикселей -
            картинка превращается в мельтешение. Эта граница уже
            проверяется в коллбеке change_scope_render_mode() через
            MAX_SHIFT_PER_FRAME, и при слишком быстрой развёртке
            переключение в roll не происходит.

    */

    scope_render_ctx* render_parameters = &used_scope->scope_render_data;
    scope_gui_basic_parameters* gui_parameters = &render_parameters->gui_parameters;

    scope_buffer_ctx* buffer = &used_scope->signal_control_data.scope_buffer_data;


    if (buffer->count < 8) return;


    int width_pixels =
        gui_parameters->display_width_units *
        render_parameters->basic_pixels_quantity_in_equivalent_unit;


    double whole_screen_time = get_whole_screen_time(used_scope);

    if (!(whole_screen_time > 0.0) || !isfinite(whole_screen_time)) return;


    double pixel_time = whole_screen_time / (double)width_pixels;

    double pixel_signal = get_pixel_signal_scale(used_scope);


    int newest_idx = (buffer->head + BUFFER_SIZE - 1) % BUFFER_SIZE;

    double newest_time = buffer->samples[newest_idx].time;


    fill_render_points_by_time_window(

        used_scope,
        render_data,
        newest_time - whole_screen_time,
        pixel_time,
        pixel_signal,
        width_pixels

    );

    render_data->trigger_locked = false;


    if (TEST_MODE_7)
    {
        printf("\n\nПостроили контекст для roll-режима!\n\n");
    }
}


void build_fixed_period_render(Scope* used_scope, signal_render_ctx* render_data)
{
    if (TEST_MODE_7)
    {
        printf("\n\nНачинаем строить рендер-контекст для фиксированного количества периодов\n\n");
    }


    /*

        Делаем всё то же самое, что и для фикс тайма, но опираемся на
        длительность конкретного количества периодов. При этом вместо
        значения времени одного юнита на дисплее пишется, сколько
        периодов показываем.

    */

    scope_main_settings_ctx* settings = &used_scope->main_settings;

    scope_measured_signal_data_ctx* measured =
        &used_scope->signal_control_data.measured_signal_characteristics;


    double whole_screen_time =
        (double)settings->periods_to_display * (double)measured->measured_period;


    // Период ещё не измерен или измерен неверно - показываем окно
    // по обычным настройкам времени, чтобы экран не оставался пустым
    if (!(whole_screen_time > 0.0) || !isfinite(whole_screen_time))
    {
        whole_screen_time = get_whole_screen_time(used_scope);
    }


    build_triggered_render(used_scope, render_data, whole_screen_time);


    if (TEST_MODE_7)
    {
        printf("\n\nПостроили контекст для фиксированного количества периодов!\n\n");
    }
}

// =========================================================================================== SIGNAL SWEEP BUILDING



void scope_signal_info_gui_renew(Scope* used_scope)
{

}


void draw_signal(Scope* used_scope, SDL_Renderer* renderer)
{

    if (TEST_MODE_7)
    {
        printf("\n\nНачинаем рисовать сигнал!\n\n");
    }

    if (!used_scope || !renderer)
        return;

    signal_render_ctx* signal =
        &used_scope->scope_render_data.signal_render_data;

    SDL_Color color = hex_to_sdl_color("#f60505", 255);
    // или
    // SDL_Color color = used_scope->scope_render_data.main_color_5;

    const int thickness = 3;


    /*

        Луч рисуется в два прохода по каждой точке.

        1) Вертикальный штрих min/max.

           Если в пиксель попало несколько сэмплов, одна точка не описывает
           происходившее внутри него. Штрих от минимума до максимума
           показывает весь диапазон, который сигнал успел пройти за время
           этого пикселя. Ровно так рисует луч любой цифровой осциллограф,
           и именно это делает быстрый сигнал на медленной развёртке
           похожим на сплошную заливку, а не на случайную ломаную.

        2) Соединительная линия до следующей точки.

           Нужна на быстрых развёртках, где в пиксель попадает меньше
           одного сэмпла и штриха просто нет.

    */

    for (int i = 0; i < signal->size; i++)
    {
        signal_render_point* current_point = &signal->points[i];

        if (!current_point->show) continue;


        // Вертикальный штрих min/max

        if (current_point->y_max != current_point->y_min)
        {
            my_sdl_draw_line(
                renderer,
                current_point->x, current_point->y_max,
                current_point->x, current_point->y_min,
                thickness,
                color
            );
        }


        // Соединение со следующей точкой

        if (i + 1 >= signal->size) break;

        signal_render_point* next_point = &signal->points[i + 1];

        if (!next_point->show) continue;


        my_sdl_draw_line(
            renderer,
            current_point->x, current_point->y,
            next_point->x, next_point->y,
            thickness,
            color
        );
    }

    if (TEST_MODE_7)
    {
        printf("\n\nСигнал нарисован!\n\n");
    }
}


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


    if (used_scope->main_settings.current_state != ON_SS) return;
    

    // Наоборот увеличиваем, чтобы уменьшиться в развертке

    if (used_scope->main_settings.signal_val_in_one_unit != 100)
    {
        used_scope->main_settings.signal_val_in_one_unit += 1;

        printf("Signal val in 1 unit now: %d\n\n", used_scope->main_settings.signal_val_in_one_unit);
    }
}


void increase_scope_value_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    // Наоборот уменьшаем, чтобы увеличиться в развертке
    if (used_scope->main_settings.signal_val_in_one_unit != 1)
    {
        used_scope->main_settings.signal_val_in_one_unit -= 1;

        printf("Signal val in 1 unit now: %d\n\n", used_scope->main_settings.signal_val_in_one_unit);
    }
}




void decrease_scope_time_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;

    
    if (used_scope->main_settings.current_mode == SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM)
    {
        // Current periods to show
        int* periods_to_display = &used_scope->main_settings.periods_to_display;

        if (*periods_to_display != 1) *periods_to_display -= 1;
    }

    else
    {
        used_scope->main_settings.current_time_in_unit_steps -= 1;

        if (used_scope->main_settings.current_time_in_unit_steps == LOW_LIMIT_TIUS)
        {
            used_scope->main_settings.current_time_in_unit_steps = LOW_LIMIT_TIUS + 1;
        }

        if (used_scope->main_settings.current_time_in_unit_steps == HIGH_LIMIT_TIUS)
        {
            used_scope->main_settings.current_time_in_unit_steps = HIGH_LIMIT_TIUS - 1;
        }


        /*
            LOW_LIMIT_TIUS,

            TIUS_1_US
            TIUS_10_US
            TIUS_100_US
            TIUS_500_US
            TIUS_1_MS
            TIUS_10_MS
            TIUS_100_MS
            TIUS_500_MS
            TIUS_1_S
            TIUS_2_S

            HIGH_LIMIT_TIUS
                
        */
        switch (used_scope->main_settings.current_time_in_unit_steps)
        {
            case TIUS_1_US:

                used_scope->main_settings.time_val_in_one_unit = 1;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;

            case TIUS_10_US:

                used_scope->main_settings.time_val_in_one_unit = 10;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;
        
            case TIUS_100_US:
            
                used_scope->main_settings.time_val_in_one_unit = 100;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;

            case TIUS_500_US:

                used_scope->main_settings.time_val_in_one_unit = 500;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;

            case TIUS_1_MS:

                used_scope->main_settings.time_val_in_one_unit = 1;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;

            case TIUS_10_MS:

                used_scope->main_settings.time_val_in_one_unit = 10;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;
        
            case TIUS_100_MS:
            
                used_scope->main_settings.time_val_in_one_unit = 100;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;

            case TIUS_500_MS:

                used_scope->main_settings.time_val_in_one_unit = 500;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;

            case TIUS_1_S:

                used_scope->main_settings.time_val_in_one_unit = 1;
                used_scope->main_settings.current_time_units = SECONDS_TU;
                break;
        
            case TIUS_2_S:
            
                used_scope->main_settings.time_val_in_one_unit = 2;
                used_scope->main_settings.current_time_units = SECONDS_TU;
                break;

            default:
                break;
        }
    }
}


void increase_scope_time_scale(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;

    
    if (used_scope->main_settings.current_mode == SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM)
    {
        // Current periods to show
        int* periods_to_display = &used_scope->main_settings.periods_to_display;

        if (*periods_to_display != 8) *periods_to_display += 1;
    }

    else
    {
        used_scope->main_settings.current_time_in_unit_steps += 1;

        if (used_scope->main_settings.current_time_in_unit_steps == LOW_LIMIT_TIUS)
        {
            used_scope->main_settings.current_time_in_unit_steps = LOW_LIMIT_TIUS + 1;
        }

        if (used_scope->main_settings.current_time_in_unit_steps == HIGH_LIMIT_TIUS)
        {
            used_scope->main_settings.current_time_in_unit_steps = HIGH_LIMIT_TIUS - 1;
        }


        /*
            LOW_LIMIT_TIUS,

            TIUS_1_US
            TIUS_10_US
            TIUS_100_US
            TIUS_500_US
            TIUS_1_MS
            TIUS_10_MS
            TIUS_100_MS
            TIUS_500_MS
            TIUS_1_S
            TIUS_2_S

            HIGH_LIMIT_TIUS
                
        */
        switch (used_scope->main_settings.current_time_in_unit_steps)
        {
            case TIUS_1_US:

                used_scope->main_settings.time_val_in_one_unit = 1;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;

            case TIUS_10_US:

                used_scope->main_settings.time_val_in_one_unit = 10;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;
        
            case TIUS_100_US:
            
                used_scope->main_settings.time_val_in_one_unit = 100;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;

            case TIUS_500_US:

                used_scope->main_settings.time_val_in_one_unit = 500;
                used_scope->main_settings.current_time_units = MICROSECONDS_TU;
                break;

            case TIUS_1_MS:

                used_scope->main_settings.time_val_in_one_unit = 1;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;

            case TIUS_10_MS:

                used_scope->main_settings.time_val_in_one_unit = 10;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;
        
            case TIUS_100_MS:
            
                used_scope->main_settings.time_val_in_one_unit = 100;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;

            case TIUS_500_MS:

                used_scope->main_settings.time_val_in_one_unit = 500;
                used_scope->main_settings.current_time_units = MILLISECONDS_TU;
                break;

            case TIUS_1_S:

                used_scope->main_settings.time_val_in_one_unit = 1;
                used_scope->main_settings.current_time_units = SECONDS_TU;
                break;
        
            case TIUS_2_S:
            
                used_scope->main_settings.time_val_in_one_unit = 2;
                used_scope->main_settings.current_time_units = SECONDS_TU;
                break;

            default:
                break;
        }
    }
}



void decrease_amplitude(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    if (used_scope->signal_control_data.controlled_signal->amplitude != 1)
        used_scope->signal_control_data.controlled_signal->amplitude -= 1;

}


void increase_amplitude(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    if (used_scope->signal_control_data.controlled_signal->amplitude != 25)
        used_scope->signal_control_data.controlled_signal->amplitude += 1;

}


void decrease_frequency(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    if (used_scope->signal_control_data.controlled_signal->frequency >= 10)
        used_scope->signal_control_data.controlled_signal->frequency -= 10;
    else
        used_scope->signal_control_data.controlled_signal->frequency = 10;
}


void increase_frequency(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    if (used_scope->signal_control_data.controlled_signal->frequency <= 20000)
        used_scope->signal_control_data.controlled_signal->frequency += 10;
    else
        used_scope->signal_control_data.controlled_signal->frequency = 20000;
}



void change_scope_render_mode(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    /*

        ИСПРАВЛЕНО: цикл режимов.

        Было:

            if (current_mode != LIMIT_SRM) current_mode += 1;
            else current_mode = 1;

        LIMIT_SRM равен 3, а рабочих режимов три: 0, 1, 2. Из режима 2
        код уходил в 3, то есть в САМ LIMIT_SRM - невалидное состояние,
        в котором main_screen_renew() попадал в default и не строил
        развёртку вообще: экран просто гас на один цикл. А при следующем
        нажатии режим ставился в 1, поэтому режим 0 больше никогда
        не становился доступен.

        Теперь честная цикличность по модулю.

    */

    used_scope->main_settings.current_mode =
        (used_scope->main_settings.current_mode + 1) % LIMIT_SRM;


    // Меняется геометрия окна - прошлый кадр для temporal-фильтра
    // больше не годится
    used_scope->scope_render_data.signal_render_data.persistence_valid = false;


    if (TEST_MODE_8)
    {
        char* mode_names[] = {

            // ИСПРАВЛЕНО: после первой строки не было запятой, поэтому
            // литералы склеивались и в массиве оказывалось два элемента
            // вместо трёх - а индекс доходил до 2
            "FIXED TIME MODE",
            "SCROLL MODE",
            "FIXED PERIOD MODE",
        };

        int curr_mode;

        switch (used_scope->main_settings.current_mode)
        {
            case SCOPE_MODE_FIXED_TIME_STEP_SRM:

                curr_mode = 0;
                break;

            case SCOPE_MODE_SCROLL_TO_RIGHT_SRM:

                curr_mode = 1;
                break;

            case SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM:

                curr_mode = 2;
                break;

            default:   

                curr_mode = 0;
                break;
            
        }

        printf("\n\nПопытка смены режима рендера на: %s\n\n", mode_names[curr_mode]);
    }


    // Слишком большая частота для степ режима
    // dx = f * Tдисплея * w / Nпер

    const float MAX_SHIFT_PER_FRAME = 10.0f;

    float screen_width = used_scope->scope_render_data.gui_parameters.display_width_units *
        used_scope->scope_render_data.basic_pixels_quantity_in_equivalent_unit;

    float frame_time = 1.0f / 30.0f;

    float time_coefficient;

    switch (used_scope->main_settings.current_time_units)
    {

        case MICROSECONDS_TU:
            time_coefficient = 1e-6;
            break;

        case MILLISECONDS_TU:
            time_coefficient = 1e-3;
            break;

        case SECONDS_TU:
            time_coefficient = 1.0f;
            break;

        default:
            time_coefficient = 1.0f;
            break;
    }


    float displayed_time = time_coefficient * used_scope->main_settings.time_val_in_one_unit *
        used_scope->scope_render_data.gui_parameters.display_width_units;
    

    if (displayed_time <= 0.0f)
    {
        used_scope->main_settings.current_mode = SCOPE_MODE_FIXED_TIME_STEP_SRM;

        if (TEST_MODE_8)
        {
            printf("\n\nСлишком маленькое время для рендера, сброс в режим 1\n\n");
        }

        return;
    }


    float shift_per_frame = screen_width * frame_time / displayed_time;

    if (

        (used_scope->main_settings.current_mode == SCOPE_MODE_SCROLL_TO_RIGHT_SRM) &&
        (shift_per_frame > MAX_SHIFT_PER_FRAME)

    )
    {
        // Roll-режим при таком масштабе времени будет мельтешить -
        // проскакиваем его и уходим в режим фиксированного числа периодов
        used_scope->main_settings.current_mode = SCOPE_MODE_SHOW_N_SIGNAL_PERIODS_SRM;

        if (TEST_MODE_8)
        {
            printf("\n\nСлишком большая скорость сдвига для рендера, переход в режим фикс. периода\n\n");
        }
    }
}



void change_controlled_signal(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;
    

    printf("signal changing");

    // scope_signal_buffer_clear(used_scope);

    used_scope->signal_control_data.type_of_controlled_signal += 1;


    // ИСПРАВЛЕНО: при достижении предела тип сбрасывался в 1 (NOISED_CST),
    // а не в 0 (CLEAN_CST). После первого же нажатия чистый сигнал
    // становился недостижим - кнопка "SIGNAL" перещёлкивала форму волны,
    // но осциллограф навсегда оставался на зашумлённом входе
    if (used_scope->signal_control_data.type_of_controlled_signal >= LIMIT_CST)
    {
        used_scope->signal_control_data.type_of_controlled_signal = CLEAN_CST;
    }

    used_scope->signal_control_data.controlled_signal->clean_or_noise = 
        !used_scope->signal_control_data.controlled_signal->clean_or_noise;

}


void play_controlled_signal(Button* btn)
{
    Scope* used_scope = (Scope*)btn->user_data;


    if (used_scope->main_settings.current_state != ON_SS) return;


    used_scope->signal_control_data.controlled_signal->audio_output_enabled = 
        !used_scope->signal_control_data.controlled_signal->audio_output_enabled;
    
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

        used_scope->signal_control_data.controlled_signal->audio_output_enabled = false;


        if (TEST_MODE_6)
        {
            printf("\n\n================ MAIN BUFFER ================\n");

            scope_buffer_ctx* buffer =
                &used_scope->signal_control_data.scope_buffer_data;


            for (int i = 0; i < 64; i++)
            {
                printf("[%3d] value=%8.5f  time=%10.6f\n",
                    i,
                    buffer->samples[i].value,
                    buffer->samples[i].time);
            }

            
            printf("mb count = %u\n\n", buffer->count);


            printf("\n\n================ HALFWAVES ================\n");

            wave_pattern_detector_ctx* wpd =
                &used_scope->signal_control_data.wave_pattern_detector_data;

            for (int i = 0; i < PERIOD_DETECTOR_BUFFER_SIZE; i++)
            {
                halfwave_data_ctx* hw = &wpd->halfwaves_for_detection[i];

                printf("[%2d] type=%d  start=%8.5f  end=%8.5f  "
                    "peak=%8.5f  trough=%8.5f  "
                    "area=%8.5f  speed=%8.5f\n",
                    i,
                    hw->halfwave_type,
                    hw->start_time,
                    hw->end_time,
                    hw->peak_value,
                    hw->trough_value,
                    hw->halfwave_area,
                    hw->halfwave_smoothed_speed);
            }


            printf("hb count = %u\n\n", wpd->count);

            printf("\n\n FREQ BEFORE OFF = %f\n\n", used_scope->signal_control_data.measured_signal_characteristics.measured_frequency);
            printf("\n\n PERIOD BEFORE OFF = %f\n\n", used_scope->signal_control_data.measured_signal_characteristics.measured_period);
        
        
        }


        scope_main_settings_clear(used_scope);

        scope_signal_buffer_clear(used_scope);
        scope_running_signal_characteristics_clear(used_scope);
        scope_measured_signal_characteristics_clear(used_scope);
        scope_filter_clear(used_scope);
        scope_peaks_ctx_clear(used_scope);
        scope_wave_pattern_detector_former_clear(used_scope);
        scope_wave_pattern_detector_clear(used_scope);


        scope_screen_gui_clear(used_scope);

        

        used_scope->scope_render_data.scope_on_off_button.pressed_color = used_scope->scope_render_data.main_color_5;



        if (TEST_MODE_6)
        {
            printf("\n\n================ MAIN BUFFER ================\n");

            scope_buffer_ctx* buffer =
                &used_scope->signal_control_data.scope_buffer_data;


            for (int i = 0; i < 64; i++)
            {
                printf("[%3d] value=%8.5f  time=%10.6f\n",
                    i,
                    buffer->samples[i].value,
                    buffer->samples[i].time);
            }

            printf("mb count = %u\n\n", buffer->count);


            printf("\n\n================ HALFWAVES ================\n");

            wave_pattern_detector_ctx* wpd =
                &used_scope->signal_control_data.wave_pattern_detector_data;

            for (int i = 0; i < PERIOD_DETECTOR_BUFFER_SIZE; i++)
            {
                halfwave_data_ctx* hw = &wpd->halfwaves_for_detection[i];

                printf("[%2d] type=%d  start=%8.5f  end=%8.5f  "
                    "peak=%8.5f  trough=%8.5f  "
                    "area=%8.5f  speed=%8.5f\n",
                    i,
                    hw->halfwave_type,
                    hw->start_time,
                    hw->end_time,
                    hw->peak_value,
                    hw->trough_value,
                    hw->halfwave_area,
                    hw->halfwave_smoothed_speed);
            }

            
            printf("hb count = %u\n\n", wpd->count);


            printf("\n\n FREQ AFTER OFF = %f\n\n", used_scope->signal_control_data.measured_signal_characteristics.measured_frequency);
            printf("\n\n PERIOD AFTER OFF = %f\n\n", used_scope->signal_control_data.measured_signal_characteristics.measured_period);
        }


    }
}
// =========================================================================================== Scope buttons callbacks


// =========================================================================================== SCOPE RENDER

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

        // Текущие текстбоксы информации о сигнале
        Textbox_render(&used_scope->scope_render_data.signal_scale_textbox, used_scope->scope_render_data.renderer);

        Textbox_render(&used_scope->scope_render_data.time_scale_textbox, used_scope->scope_render_data.renderer);

        Textbox_render(&used_scope->scope_render_data.frequency_or_period_textbox, used_scope->scope_render_data.renderer);

        Textbox_render(&used_scope->scope_render_data.amplitude_textbox, used_scope->scope_render_data.renderer);



        main_screen_renew(used_scope);

        // Рисуем сигнал
        draw_signal(used_scope, used_scope->scope_render_data.renderer);

    }

}

// =========================================================================================== SCOPE RENDER
