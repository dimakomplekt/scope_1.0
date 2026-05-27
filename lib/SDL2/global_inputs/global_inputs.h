// global_inputs.h

#pragma once

// =========================================================================================== IMPORT

#include "../SDL2-2.30.8/x86_64-w64-mingw32/include/SDL2/SDL.h"
#include <stdbool.h>

// =========================================================================================== IMPORT


// =========================================================================================== ACTIONS

typedef enum
{

    KEY_ENTER,
    KEY_EXIT,
    KEY_MENU_FORWARD,
    KEY_MENU_BACK,

    KEY_SPECIAL_1,

    KEY_LEFT,
    KEY_UP,
    KEY_RIGHT,
    KEY_DOWN,

    KEY_ACTION_COUNT

} key_action;

// =========================================================================================== ACTIONS


// =========================================================================================== BUTTON STATE

typedef struct
{

    bool current;
    bool previous;

} GI_button_state;

// =========================================================================================== BUTTON STATE


// =========================================================================================== MOUSE

typedef struct
{

    float x;
    float y;

    GI_button_state lb;
    GI_button_state rb;

} GI_mouse;

// =========================================================================================== MOUSE


// =========================================================================================== KEYBOARD

typedef struct
{

    bool current[SDL_NUM_SCANCODES];
    bool previous[SDL_NUM_SCANCODES];

} GI_keyboard;

// =========================================================================================== KEYBOARD


// =========================================================================================== INPUT MANAGER

typedef struct
{
    
    GI_mouse mouse;
    GI_keyboard keyboard;

    SDL_Scancode keymap[KEY_ACTION_COUNT];

} GI_input_manager;

// =========================================================================================== INPUT MANAGER


// =========================================================================================== GLOBAL

extern GI_input_manager App_inputs;

// =========================================================================================== GLOBAL


// =========================================================================================== MAIN API

void GI_init(void);

void GI_update(void);

// =========================================================================================== MAIN API


// =========================================================================================== KEYBOARD API

bool GI_key_pressed(SDL_Scancode key);

bool GI_key_held(SDL_Scancode key);

bool GI_key_just_released(SDL_Scancode key);

bool GI_key_released(SDL_Scancode key);

// =========================================================================================== KEYBOARD API


// =========================================================================================== ACTION API

bool GI_action_pressed(key_action action);

bool GI_action_held(key_action action);

bool GI_action_just_released(key_action action);

bool GI_action_released(key_action action);

// =========================================================================================== ACTION API


// =========================================================================================== MOUSE API

float GI_mouse_x(void);

float GI_mouse_y(void);


// ===== LEFT BUTTON =====

bool GI_lb_pressed(void);

bool GI_lb_held(void);

bool GI_lb_just_released(void);

bool GI_lb_released(void);


// ===== RIGHT BUTTON =====

bool GI_rb_pressed(void);

bool GI_rb_held(void);

bool GI_rb_just_released(void);

bool GI_rb_released(void);

// =========================================================================================== MOUSE API
