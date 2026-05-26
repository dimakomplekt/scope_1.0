// app_timer.c


// =========================================================================================== IMPORT

#include "app_timer.h"

// =========================================================================================== IMPORT


// =========================================================================================== TIMER SINGLETON

static app_timer App_timer = {0};

// =========================================================================================== TIMER SINGLETON


// =========================================================================================== API REALIZATION

void app_timer_init(void)
{
    if (App_timer.initialized) return;

    App_timer.last_ticks = SDL_GetPerformanceCounter();
    App_timer.current_time = 0.0;
    App_timer.delta_time = 0.0;
    App_timer.initialized = 1;
}


void app_timer_update(void)
{
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();

    double dt = (double)(now - App_timer.last_ticks) / (double)freq;

    App_timer.delta_time = dt;
    App_timer.current_time += dt;

    App_timer.last_ticks = now;
}


double app_timer_get_time(void)
{
    return App_timer.current_time;
}

double app_timer_get_delta(void)
{
    return App_timer.delta_time;
}

// =========================================================================================== API REALIZATION