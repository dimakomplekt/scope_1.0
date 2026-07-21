// app_timer.c


// =========================================================================================== IMPORT

#include "app_timer.h"

// =========================================================================================== IMPORT

app_timer App_timer = {0};


simulation_timer Simulation_timer = {0};

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
    uint64_t frequency = SDL_GetPerformanceFrequency();

    double dt = (double)(now - App_timer.last_ticks) / (double)frequency;

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



void simulation_timer_init(void)
{
    if (Simulation_timer.initialized) return;

    Simulation_timer.current_time = 0.0;
    Simulation_timer.time_step = 1.0 * SIM_BUFFER_SIZE /  SIM_SAMPLE_RATE;

    Simulation_timer.sample_delta_time = 1.0 / SIM_SAMPLE_RATE;


    Simulation_timer.initialized = 1;
}


void simulation_timer_update(void)
{
    Simulation_timer.current_time += Simulation_timer.time_step;
}


double simulation_timer_get_time(void)
{
    return Simulation_timer.current_time;
}



double simulation_timer_get_time_step(void)
{
    return Simulation_timer.time_step;
}


double simulation_timer_get_sample_step(void)
{
    return Simulation_timer.sample_delta_time;
}

// =========================================================================================== API REALIZATION
