// sin_generator.c

// =========================================================================================== IMPORT

#include "sin_generator.h"
#include <math.h>
#include <stdlib.h>

// =========================================================================================== IMPORT


// =========================================================================================== GENERATOR SINGLETON

sin_generator_ctx Oscillator_1 = {0};

// =========================================================================================== GENERATOR SINGLETON


// =========================================================================================== API REALIZATION


void sin_generator_init(float amplitude, float frequency)
{
    if (Oscillator_1.initialized) return;

    Oscillator_1.amplitude = amplitude;
    Oscillator_1.frequency = frequency;
    Oscillator_1.initialized = 1;
}


void sin_generator_update(void)
{
    double t = app_timer_get_time();

    // CLEAN SIGNAL
    Oscillator_1.current_clean = Oscillator_1.amplitude * sin(2.0 * M_PI * Oscillator_1.frequency * t);

    // NOISE SIGNAL (простая модель шума)
    // Генерация симметричного белого шума:
    // rand() даёт значение [0 .. RAND_MAX], нормализуем в [0..1],
    // затем сдвигаем в [-0.5 .. +0.5] и масштабируем амплитудой.
    //
    // Итоговый диапазон шума:
    // noise ∈ [-0.1 .. +0.1] при множителе 0.2f
    //
    // Используется для имитации аналоговой нестабильности сигнала
    // (дрожание измерений / "грязь" осциллографа)
    float noise = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.2f;

    Oscillator_1.current_noise = Oscillator_1.current_clean + noise;
}

float sin_generator_get_clean(void)
{
    return Oscillator_1.current_clean;
}

float sin_generator_get_noise(void)
{
    return Oscillator_1.current_noise;
}

void sin_generator_set_amplitude(float a)
{
    Oscillator_1.amplitude = a;
}

void sin_generator_set_frequency(float f)
{
    Oscillator_1.frequency = f;
}

// =========================================================================================== API REALIZATION
