// global_inputs.c

// =========================================================================================== IMPORT

#include "global_inputs.h"

#include <string.h>

// =========================================================================================== IMPORT


// =========================================================================================== GLOBAL

GI_input_manager App_inputs;

// =========================================================================================== GLOBAL


// =========================================================================================== INTERNAL

static bool button_pressed(GI_button_state* b)
{
    return b->current && !b->previous;
}

static bool button_held(GI_button_state* b)
{
    return b->current && b->previous;
}

static bool button_just_released(GI_button_state* b)
{
    return !b->current && b->previous;
}

static bool button_released(GI_button_state* b)
{
    return !b->current && !b->previous;
}

// =========================================================================================== INTERNAL


// =========================================================================================== INIT

void GI_init(void)
{
    memset(&App_inputs, 0, sizeof(App_inputs));

    // ===== DEFAULT BINDS =====

    App_inputs.keymap[KEY_ENTER] = SDL_SCANCODE_RETURN;

    App_inputs.keymap[KEY_EXIT] = SDL_SCANCODE_ESCAPE;

    App_inputs.keymap[KEY_MENU_FORWARD] = SDL_SCANCODE_RIGHT;
    App_inputs.keymap[KEY_MENU_BACK] = SDL_SCANCODE_LEFT;

    App_inputs.keymap[KEY_SPECIAL_1] = SDL_SCANCODE_SPACE;

    App_inputs.keymap[KEY_LEFT] = SDL_SCANCODE_LEFT;
    App_inputs.keymap[KEY_UP] = SDL_SCANCODE_UP;
    App_inputs.keymap[KEY_RIGHT] = SDL_SCANCODE_RIGHT;
    App_inputs.keymap[KEY_DOWN] = SDL_SCANCODE_DOWN;
}

// =========================================================================================== INIT


// =========================================================================================== UPDATE

void GI_update(void)
{
    // =======================================================================================
    // KEYBOARD
    // =======================================================================================

    const Uint8* sdl_keys = SDL_GetKeyboardState(NULL);

    for (int i = 0; i < SDL_NUM_SCANCODES; i++)
    {
        App_inputs.keyboard.previous[i] = App_inputs.keyboard.current[i];

        App_inputs.keyboard.current[i] = sdl_keys[i];
    }

    // =======================================================================================
    // MOUSE
    // =======================================================================================

    App_inputs.mouse.lb.previous = App_inputs.mouse.lb.current;
    App_inputs.mouse.rb.previous = App_inputs.mouse.rb.current;

    int mx;
    int my;

    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    App_inputs.mouse.x = (float)mx;
    App_inputs.mouse.y = (float)my;

    App_inputs.mouse.lb.current = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    App_inputs.mouse.rb.current = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;


    /*

        ФИЛЬТР ПО MOUSE FOCUS - НЕОБЯЗАТЕЛЬНОЕ ЗАЩИТНОЕ ИЗМЕНЕНИЕ

        Никакого наблюдавшегося бага здесь не чинится. Это страховка
        от потенциальной проблемы, а не исправление проявившейся.

        Суть: SDL_GetMouseState() отдаёт ГЛОБАЛЬНОЕ состояние кнопок мыши
        и последнее известное положение курсора - независимо от того,
        находится ли курсор над нашим окном. Значит в принципе возможна
        ситуация, когда клик в чужом окне совпадает по координатам
        с нашей кнопкой и вызывает её коллбек.

        SDL_GetMouseFocus() возвращает окно, над которым сейчас курсор,
        или NULL. Если курсора над нашими окнами нет - гасим состояние
        кнопок. Позицию при этом оставляем как есть: hover-проверки
        всё равно её отбракуют, а обнулять координаты нельзя, иначе
        кнопка в точке (0;0) считалась бы наведённой.

        На обычную работу не влияет: при клике по кнопке курсор
        по определению находится над окном. Если этот файл не нужен -
        его можно просто не переносить в проект, остальные изменения
        от него не зависят.

    */

    if (!SDL_GetMouseFocus())
    {
        App_inputs.mouse.lb.current = false;
        App_inputs.mouse.rb.current = false;
    }
}

// =========================================================================================== UPDATE


// =========================================================================================== KEYBOARD API

bool GI_key_pressed(SDL_Scancode key)
{
    return App_inputs.keyboard.current[key]
        && !App_inputs.keyboard.previous[key];
}

bool GI_key_held(SDL_Scancode key)
{
    return App_inputs.keyboard.current[key]
        && App_inputs.keyboard.previous[key];
}

bool GI_key_just_released(SDL_Scancode key)
{
    return !App_inputs.keyboard.current[key]
        && App_inputs.keyboard.previous[key];
}

bool GI_key_released(SDL_Scancode key)
{
    return !App_inputs.keyboard.current[key]
        && !App_inputs.keyboard.previous[key];
}

// =========================================================================================== KEYBOARD API


// =========================================================================================== ACTION API

bool GI_action_pressed(key_action action)
{
    return GI_key_pressed(App_inputs.keymap[action]);
}

bool GI_action_held(key_action action)
{
    return GI_key_held(App_inputs.keymap[action]);
}

bool GI_action_just_released(key_action action)
{
    return GI_key_just_released(App_inputs.keymap[action]);
}

bool GI_action_released(key_action action)
{
    return GI_key_released(App_inputs.keymap[action]);
}

// =========================================================================================== ACTION API


// =========================================================================================== MOUSE API

float GI_mouse_x(void)
{
    return App_inputs.mouse.x;
}

float GI_mouse_y(void)
{
    return App_inputs.mouse.y;
}


// =========================================================================================== LEFT BUTTON

bool GI_lb_pressed(void)
{
    return button_pressed(&App_inputs.mouse.lb);
}

bool GI_lb_held(void)
{
    return button_held(&App_inputs.mouse.lb);
}

bool GI_lb_just_released(void)
{
    return button_just_released(&App_inputs.mouse.lb);
}

bool GI_lb_released(void)
{
    return button_released(&App_inputs.mouse.lb);
}

// =========================================================================================== LEFT BUTTON


// =========================================================================================== RIGHT BUTTON

bool GI_rb_pressed(void)
{
    return button_pressed(&App_inputs.mouse.rb);
}

bool GI_rb_held(void)
{
    return button_held(&App_inputs.mouse.rb);
}

bool GI_rb_just_released(void)
{
    return button_just_released(&App_inputs.mouse.rb);
}

bool GI_rb_released(void)
{
    return button_released(&App_inputs.mouse.rb);
}

// =========================================================================================== RIGHT BUTTON