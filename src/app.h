// app.h

#pragma once

// =========================================================================================== IMPORT

#ifdef _WIN32
#include <windows.h>
#endif


#include "../lib/engine.h"

#include "../lib/SDL2/my_sdl_draw/my_sdl_draw.h"
#include "../lib/SDL2/ui.h"


#include "../lib/app_timer/app_timer.h"
#include "../lib/scope/scope.h"
#include "../lib/sin_generator/sin_generator.h"

#include "global_data.h"

// =========================================================================================== IMPORT




// =========================================================================================== TYPES

// Контекст приложения
typedef struct SDL_app_ctx
{
    const char* name;

    int running;

    SDL_Window* window;
    SDL_Renderer* renderer;

} SDL_app_ctx;


// =========================================================================================== TYPES

// =========================================================================================== PREDECLARE APP API


// Настройка консоли под вывод кириллицы
void cyrillic_console_setup();

// INIT / LOOP API

int SDL_app_init_and_run();

int  SDL_app_init(SDL_app_ctx* app, int w, int h, const char* title);
int  SDL_app_run(SDL_app_ctx* app);
void SDL_app_shutdown(SDL_app_ctx* app);


// INTERNAL LOOP PARTS

void SDL_app_handle_events(SDL_app_ctx* app);
void SDL_app_update(SDL_app_ctx* app);
void SDL_app_render(SDL_app_ctx* app);

// =========================================================================================== PREDECLARE APP API
