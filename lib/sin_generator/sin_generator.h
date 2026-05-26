// sin_generator.h

#pragma once


// =========================================================================================== IMPORT

#include "../engine.h"
#include "../app_timer/app_timer.h"
#include <stdint.h>

// =========================================================================================== IMPORT


// =========================================================================================== GENERATOR STRUCT

typedef struct sin_generator
{

    float amplitude;
    float frequency;

    float current_clean;
    float current_noise;

    int initialized;

} sin_generator;

// =========================================================================================== GENERATOR STRUCT


// =========================================================================================== GENERATOR API

// init oscillator
void sin_generator_init(float amplitude, float frequency);

// update (берёт время из app_timer)
void sin_generator_update(void);

// getters
float sin_generator_get_clean(void);
float sin_generator_get_noise(void);

// optional setters
void sin_generator_set_amplitude(float a);
void sin_generator_set_frequency(float f);

// =========================================================================================== GENERATOR API