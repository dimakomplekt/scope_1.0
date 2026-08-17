// my_generator.c

// =========================================================================================== IMPORT

#include "my_generator.h"
#include <math.h>
#include <stdlib.h>

#include <stdio.h>

// =========================================================================================== IMPORT


// =========================================================================================== GENERATOR SINGLETON

my_generator_ctx Oscillator_1 = {0};

// =========================================================================================== GENERATOR SINGLETON


// =========================================================================================== HELPER

float rand_float(float min, float max) {

    // 1. Получаем случайное число от 0.0 до 1.0
    float scale = (float)rand() / (float)RAND_MAX; 
    
    // 2. Масштабируем его под нужный диапазон и сдвигаем на min
    return min + scale * (max - min);
}

// =========================================================================================== HELPER



// =========================================================================================== API REALIZATION


void my_generator_init(float amplitude, float frequency)
{
    if (Oscillator_1.initialized) return;

    Oscillator_1.amplitude = amplitude;
    Oscillator_1.frequency = frequency;
    Oscillator_1.initialized = 1;

    Oscillator_1.clean_or_noise = true;


    Oscillator_1.wave_type_number = 0;


    
    SDL_AudioSpec spec = {0};

    spec.freq = (int)SIM_SAMPLE_RATE;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = SIM_BUFFER_SIZE;
    spec.callback = audio_callback;
    spec.userdata = NULL;


    Oscillator_1.audio_device =
        SDL_OpenAudioDevice(
            NULL,
            0,
            &spec,
            NULL,
            0
        );


    if (Oscillator_1.audio_device == 0)
    {
        printf(
            "Audio init error: %s\n",
            SDL_GetError()
        );

        return;
    }


    SDL_PauseAudioDevice(
        Oscillator_1.audio_device,
        0
    );


    Oscillator_1.prev_gen_time = 0.0f;
}


void my_generator_update(void)
{
    // Смотрим в "прошлое", относительно текущего тика симуляции
    double curr_time = simulation_timer_get_time() - simulation_timer_get_time_step();

    // CLEAN SIGNAL
    double dt = simulation_timer_get_sample_step();


    for (int i = 0; i < SIM_BUFFER_SIZE; i++)
    {
        double t =

            Oscillator_1.prev_gen_time +
            (i + 1) * dt;

        
        // =====================================================================
        // Выбор формы волны
        // =====================================================================

        float phase = Oscillator_1.frequency * t;
        phase -= floorf(phase);      // 0..1

        float clean = 0.0f;

        switch (Oscillator_1.wave_type_number)
        {
            // -------------------------------------------------------------
            // 0. Синус
            // -------------------------------------------------------------

            case 0:
            {
                clean =
                    Oscillator_1.amplitude *
                    sinf(2.0f * M_PI * phase);

                break;
            }


            /*
            // -------------------------------------------------------------
            // 0. Какая-то тяжелая волна
            // -------------------------------------------------------------

            case 1:
            {

                if (phase < 0.1f) clean = Oscillator_1.amplitude;
                else if (phase < 0.2f)
                {
                    clean =
                        Oscillator_1.amplitude *
                        (4.0f * fabsf(phase - 0.5f) - 1.0f);
                }
                else if (phase < 0.4f)
                {
                    clean =
                        -Oscillator_1.amplitude + Oscillator_1.amplitude *
                        (4.0f * fabsf(phase - 0.5f) - 1.0f);
                }

                else if (phase < 0.6f)
                {
                    clean =
                        -(-Oscillator_1.amplitude + Oscillator_1.amplitude *
                        (4.0f * fabsf(phase - 0.5f) - 1.0f));
                }
                else if (phase < 0.7f) clean = Oscillator_1.amplitude * 0.5;
                else if (phase < 0.8f) clean = - Oscillator_1.amplitude * 0.8;
                else if (phase < 0.9f) clean = Oscillator_1.amplitude * 0.7;
                else if (phase < 1.0f) clean = - Oscillator_1.amplitude * 0.3;

                break;
            }

            // -------------------------------------------------------------
            // 2. Пила
            // -------------------------------------------------------------
            case 2:
            {
                clean =
                    Oscillator_1.amplitude *
                    (2.0f * phase - 1.0f);
                break;
            }

            // -------------------------------------------------------------
            // 3. Квадрат
            // -------------------------------------------------------------
            case 3:
            {
                clean =
                    (phase < 0.5f) ?
                    Oscillator_1.amplitude :
                    -Oscillator_1.amplitude;
                break;
            }

            // -------------------------------------------------------------
            // 4. Треугольник
            // -------------------------------------------------------------
            case 4:
            {
                clean =
                    Oscillator_1.amplitude *
                    (4.0f * fabsf(phase - 0.5f) - 1.0f);
                break;
            }

            // -------------------------------------------------------------
            // 5. Полусинус + полка
            // -------------------------------------------------------------
            case 5:
            {
                if (phase < 0.5f)
                    clean =
                        Oscillator_1.amplitude *
                        sinf(phase * 2.0f * M_PI);
                else
                    clean =
                        -0.4f * Oscillator_1.amplitude;

                break;
            }

            // -------------------------------------------------------------
            // 6. Двойной пик
            // -------------------------------------------------------------
            case 6:
            {
                clean =
                    Oscillator_1.amplitude *
                    sinf(4.0f * M_PI * phase);

                break;
            }

            // -------------------------------------------------------------
            // 7. Ступеньки
            // -------------------------------------------------------------
            case 7:
            {
                if (phase < 0.2f) clean = -1.0f;
                else if (phase < 0.4f) clean = -0.3f;
                else if (phase < 0.6f) clean = 0.2f;
                else if (phase < 0.8f) clean = 0.7f;
                else clean = 1.0f;

                clean *= Oscillator_1.amplitude;
                break;
            }

            // -------------------------------------------------------------
            // 8. Твоя мультиволна
            // -------------------------------------------------------------
            case 8:
            {
                if (phase < 0.2f)
                    clean = -5.0f;
                else if (phase < 0.4f)
                    clean = 5.0f;
                else if (phase < 0.6f)
                    clean = 0.0f;
                else if (phase < 0.8f)
                    clean = -2.0f;
                else
                    clean = 2.0f;

                break;
            }

            // -------------------------------------------------------------
            // 9. Полуволновой синус
            // -------------------------------------------------------------
            case 9:
            {
                float s = sinf(2.0f * M_PI * phase);

                if (s < 0.0f)
                    s = 0.0f;

                clean =
                    Oscillator_1.amplitude *
                    s;

                break;
            }

            // -------------------------------------------------------------
            // 10. FM-подобная волна
            // -------------------------------------------------------------
            case 10:
            {
                clean =
                    Oscillator_1.amplitude *
                    sinf(
                        2.0f * M_PI * phase +
                        2.0f * sinf(8.0f * M_PI * phase)
                    );

                break;
            }
            */

            default:
            {
                clean = 0.0f;
                break;
            }
        }

        Oscillator_1.current_clean[i] = clean;

        
        // if (i < 5)
        // {
        // printf("\n%d  t=%f  clean=%f\n", i, t, clean);
        // }


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
        
        // BASIC:
        // Oscillator_1.current_noise[i] = clean + noise;


        // 1. Белый шум 
        float random_noise = rand_float(-1.0f, 1.0f);
        float random_phase = rand_float(0.2, 0.8);
        
        // Генерим нойз только иногда - когда фаза попадает в окно +- 0.2 от рандомной фазы
        if (!((phase >= (random_phase - 0.2)) || (phase <= (random_phase + 0.2)))) 
            random_noise = 0.0f;

        // 2. Синусоидальная помеха заданной частоты

        // НАСТРОЙКА ЧАСТОТЫ ПОМЕХИ

        float noise_freq  = 30.0f; 
        float noise_amp = 0.5f;
        float noise_offset = -0.1f;

        float harmonic_noise = noise_offset + noise_amp * sin(2.0 * M_PI * noise_freq * t);

        // Белый шум + синусоидальная помеха
        // При текущих настройках - в пределах от -0.25 до +1.45, что даёт диапазон сигнала от -0.25 до 1.45
        float full_noise = random_noise; // + harmonic_noise;

        Oscillator_1.current_noise[i] = clean + full_noise; 


        audio_ring_buffer_ctx* rb =
            &Oscillator_1.audio_buffer;


        if (1)
        {
            if (Oscillator_1.clean_or_noise)
            {
                int next_write =
                    rb->write_index + 1;


                if (next_write >= AUDIO_RING_SIZE)
                {
                    next_write = 0;
                }


                if (next_write != rb->read_index)
                {
                    rb->samples[rb->write_index] = clean / 5.0f;

                    rb->write_index = next_write;
                }
            }
            else
            {
                int next_write =
                    rb->write_index + 1;


                if (next_write >= AUDIO_RING_SIZE)
                {
                    next_write = 0;
                }


                if (next_write != rb->read_index)
                {
                    rb->samples[rb->write_index] = (clean + full_noise) / 5.0f;

                    rb->write_index = next_write;
                }
            }
        }

    }


    Oscillator_1.prev_gen_time += simulation_timer_get_time_step(); 

}


float* my_generator_get_clean(void)
{
    return Oscillator_1.current_clean;
}


float* my_generator_get_noise(void)
{
    return Oscillator_1.current_noise;
}


void my_generator_set_amplitude(float a)
{
    Oscillator_1.amplitude = a;
}

void my_generator_set_frequency(float f)
{
    Oscillator_1.frequency = f;
}


void audio_callback(
    void* userdata,
    Uint8* stream,
    int len)
{
    float* buffer = (float*)stream;

    int samples = len / sizeof(float);


    audio_ring_buffer_ctx* rb =
        &Oscillator_1.audio_buffer;


    for (int i = 0; i < samples; i++)
    {

        if (!Oscillator_1.audio_output_enabled)
        {
            buffer[i] = 0.0f;
            continue;
        }


        if (rb->read_index != rb->write_index)
        {
            buffer[i] =
                rb->samples[rb->read_index];


            rb->read_index++;


            if (rb->read_index >= AUDIO_RING_SIZE)
            {
                rb->read_index = 0;
            }
        }
        else
        {
            // буфер пуст
            buffer[i] = 0.0f;
        }

    }
}

// =========================================================================================== API REALIZATION
