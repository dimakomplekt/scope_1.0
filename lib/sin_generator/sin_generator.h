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

    double prev_call_time;
    double prev_gen_time;

    float current_clean[SAMPLES_IN_STEP];
    float current_noise[SAMPLES_IN_STEP];

    int initialized;

} sin_generator_ctx;

// =========================================================================================== GENERATOR STRUCT


// =========================================================================================== GENERATOR API

// init oscillator
void sin_generator_init(float amplitude, float frequency);

// update (берёт время из app_timer)
void sin_generator_update(void);

// getters
float* sin_generator_get_clean(void);
float* sin_generator_get_noise(void);

// optional setters
void sin_generator_set_amplitude(float a);
void sin_generator_set_frequency(float f);


extern sin_generator_ctx Oscillator_1;

// =========================================================================================== GENERATOR API