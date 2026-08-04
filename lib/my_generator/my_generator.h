// my_generator.h

#pragma once


// =========================================================================================== IMPORT

#include "../engine.h"
#include "../app_timer/app_timer.h"
#include <stdint.h>

// =========================================================================================== IMPORT



#define AUDIO_RING_SIZE 4096


typedef struct
{
    float samples[AUDIO_RING_SIZE];

    volatile int write_index;
    volatile int read_index;

} audio_ring_buffer_ctx;


// =========================================================================================== GENERATOR STRUCT

typedef struct my_generator
{

    float amplitude;
    float frequency;

    double prev_gen_time;

    float current_clean[SIM_BUFFER_SIZE];
    float current_noise[SIM_BUFFER_SIZE];

    int initialized;

    int wave_type_number;


    bool audio_output_enabled;


    audio_ring_buffer_ctx audio_buffer;
    bool clean_or_noise; // true - чистый сигнал, false - шумовой сигнал

    
    SDL_AudioDeviceID audio_device;

    SDL_AudioSpec audio_spec;

} my_generator_ctx;

// =========================================================================================== GENERATOR STRUCT


// =========================================================================================== GENERATOR API

// init oscillator
void my_generator_init(float amplitude, float frequency);

// update (берёт время из app_timer)
void my_generator_update(void);

// getters
float* my_generator_get_clean(void);
float* my_generator_get_noise(void);

// optional setters
void my_generator_set_amplitude(float a);
void my_generator_set_frequency(float f);


extern my_generator_ctx Oscillator_1;

// =========================================================================================== GENERATOR API



void audio_callback(
    void* userdata,
    Uint8* stream,
    int len);