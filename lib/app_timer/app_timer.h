// app_timer.h

#pragma once


// =========================================================================================== IMPORT

#include <stdint.h>
#include "../engine.h"

// =========================================================================================== IMPORT


// =========================================================================================== DEFINES

#define SIM_SAMPLE_RATE 48000.0
#define SIM_BUFFER_SIZE 128

// =========================================================================================== DEFINES


// =========================================================================================== TIMER

// Рилтайм
typedef struct app_timer
{
    double current_time;   // текущее время (сек)
    double delta_time;     // dt между кадрами

    uint64_t last_ticks;   // внутренний тик

    int initialized;

} app_timer;


// Псевдотаймер для симуляции
// 48000 
typedef struct simulation_timer 
{

    double current_time;
    double time_step;

    double sample_delta_time;

    int initialized;

} simulation_timer;

// =========================================================================================== TIMER


// =========================================================================================== TIMER API

// первичная инициализация
void app_timer_init(void);

// обновление (вызывать каждый кадр)
void app_timer_update(void);

// получение текущего времени
double app_timer_get_time(void);

// получение dt
double app_timer_get_delta(void);


extern app_timer App_timer;

// =========================================================================================== TIMER API



// =========================================================================================== SIMULATION TIMER API

// первичная инициализация
void simulation_timer_init(void);

// обновление (вызывать каждый кадр)
void simulation_timer_update(void);

// получение текущего времени
double simulation_timer_get_time(void);

// получение dt
double simulation_timer_get_time_step(void);


double simulation_timer_get_sample_step(void);


extern simulation_timer Simulation_timer;


// =========================================================================================== SIMULATION TIMER API
