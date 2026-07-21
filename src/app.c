// app.c

// =========================================================================================== IMPORT

#include "app.h"
#include <stdio.h>


#include "../lib/app_timer/app_timer.h"
#include "../lib/sin_generator/sin_generator.h"


// =========================================================================================== IMPORT

// =========================================================================================== DATA

Scope scope_1;

bool wave_samples_generated = true;
bool wave_samples_passed_to_scope = true;

// =========================================================================================== DATA



// =========================================================================================== REALIZATION


int SDL_app_init_and_run()
{
    SDL_app_ctx app = {0};

    // SDL App init
    if (!SDL_app_init(&app, SCREEN_WIDTH, SCREEN_HEIGHT, "Scope_1.0")) return -1;

    SDL_app_run(&app);

    SDL_app_shutdown(&app);

    return 0;
}


int SDL_app_init(SDL_app_ctx* app, int w, int h, const char* title)
{

    // SDL INIT
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return 0;

    // SDL TTF INIT
    if (TTF_Init() != 0)
        return 0;

    app->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w,
        h,
        0
    );

    if (!app->window)
        return 0;

    app->renderer = SDL_CreateRenderer(
        app->window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    if (!app->renderer)
        return 0;

        
    SDL_SetRenderDrawBlendMode(
        app->renderer,
        SDL_BLENDMODE_BLEND
    );

        
    SDL_BlendMode mode;

    SDL_GetRenderDrawBlendMode(
        app->renderer,
        &mode
    );
    
    // printf("blend mode = %d\n", mode);

    app->running = 1;
    app->name = title;

    // Инпут
    GI_init();

    // Таймеры
    app_timer_init();
    simulation_timer_init();



    scope_init(&scope_1, app->renderer);

    sin_generator_init(5.0, 300.0);
    
    signal_check(&scope_1, &Oscillator_1);

    // printf("\nScope signal A: .%f", scope_1.signal_control_data.controlled_signal->amplitude);
    // printf("\nScope signal F: .%f", scope_1.signal_control_data.controlled_signal->frequency);

    cyrillic_console_setup();

    return 1;
}


// Обработчик событий
void SDL_app_handle_events(SDL_app_ctx* app)
{
    SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            app->running = 0;
        }
    }
}


bool cycle_start_locked_1 = false;
bool cycle_start_locked_2 = false;


double start_time_1;
double start_time_2;
double start_time_3;

double curr_time;


double prev_call_time;


#define TEST_MODE_BUFFERS 0
#define TEST_MODE_ANALYSATOR 0


// Апдейт апы в котором идут апдейты всех объектов
void SDL_app_update(SDL_app_ctx* app)
{
    // Realtime update (для работы программы)
    app_timer_update();


    if (wave_samples_generated && wave_samples_passed_to_scope)
    {
        // Simulation time (для работы дискретных модулей - генератора синуса, осциллографа)
        // инкрементируется каждый update -> соотв. генератор и осциллограф получают одинаковый буффер
        simulation_timer_update();

        wave_samples_generated = false;
        wave_samples_passed_to_scope = false;
    }

    // Всегда чекаем инпуты
    GI_update();

    // Контроль цикла приложения по реальному времени
    //
    curr_time = app_timer_get_time();

    if (start_time_1 == 0) start_time_1 = curr_time;
    if (start_time_2 == 0) start_time_2 = curr_time;


    {
        // Апдейт синуса на текущем simulation time
        // получаем 128 значений на отрезке времени 1 / 48000 с
        sin_generator_update();

        wave_samples_generated = true;



        // Апдейт осциллографа на текущем simulation time
        // получаем 128 значений на отрезке времени 1 / 48000 с
        scope_fast_update(&scope_1);

        wave_samples_passed_to_scope = true;
    }
    

    if (TEST_MODE_BUFFERS)
    {
        // Чек связности генератора и буффера

        float* values_1 = sin_generator_get_clean();

        printf("\n\nScope signal value from generator:\n");


        for (int i = 0; i < SIM_BUFFER_SIZE; i++)
        {
            printf("%f; ", values_1[i]);
        }

        printf("\n");


        printf("\n\nScope signal value from scope buffer:\n");

        for (int i = 0; i < SIM_BUFFER_SIZE; i++)
        {
            printf("%f;  ", scope_1.signal_control_data.scope_buffer_data.samples[scope_1.signal_control_data.scope_buffer_data.head - SIM_BUFFER_SIZE + i].value);
        }

        printf("\n");

        printf("t = %.9f\n", app_timer_get_time());
        printf("prev_t = %.9f\n", prev_call_time);
        printf("%f\n", Oscillator_1.frequency);
    }
    

    // ==== SLOW ANALYSIS ====

    // Привязанно к рилтайму (для отсутствия коллов на каждом шаге) коллим анализ поступивших буфферов
    // для поиска характеристик сигнала в 2 раза чаще вывода на рендер
    const double STEP_120 = 1.0 / 120.0;


    if (curr_time - start_time_2 >= STEP_120)
    {
        // Чек характеристик
        scope_slow_update(&scope_1);


        if (TEST_MODE_ANALYSATOR)
        {

            if (scope_1.main_settings.current_state == ON_SS)
            {

                // printf("Scope signal value from buffer: .%f \n", scope_1.signal_control_data.scope_buffer_data.samples[scope_1.signal_control_data.scope_buffer_data.head - 1].value);
                // printf("\n\n\nScope signal by analysis A: .%f \n", scope_1.signal_control_data.measured_signal_characteristics.measured_max);
                // printf("Scope signal by analysis T: .%f \n", scope_1.signal_control_data.measured_signal_characteristics.measured_period);
                // printf("Scope signal by analysis F: .%f \n\n", scope_1.signal_control_data.measured_signal_characteristics.measured_frequency);

            }
        }

        // Сдвиг для паузы между этим чеком и следующим
        start_time_2 += STEP_120;
    }

    // ==== SLOW ANALYSIS ====
}


// Рендер апы в котором идут рендеры всех объектов
void SDL_app_render(SDL_app_ctx* app)
{ 
    curr_time = app_timer_get_time();

    if (start_time_3 == 0) start_time_3 = curr_time;

    const double STEP_60 = 1.0 / 60.0;


    // Рендеринг с ограничением частоты в 60Гц
    if (curr_time - start_time_3 >= STEP_60)
    {
        SDL_SetRenderDrawColor(app->renderer, 10, 10, 10, 255);
        SDL_RenderClear(app->renderer);

        scope_render(&scope_1);

        // printf("Scope signal Tpx from render: .%f", scope_1.scope_render_data.signal_render_data.points[1].x);
        // printf("Scope signal Vpx from render: .%f", scope_1.scope_render_data.signal_render_data.points[1].y);
        // printf("Scope signal show:  .%s\n", scope_1.scope_render_data.signal_render_data.points[1].show ? "true" : "false");


        SDL_RenderPresent(app->renderer);

        start_time_3 += STEP_60;
    }
}


// Основной луп апдейт-рендер
int SDL_app_run(SDL_app_ctx* app)
{
    while (app->running)
    {
        SDL_app_handle_events(app);

        // Апдейт и рендер апы
        SDL_app_update(app);
        SDL_app_render(app);
        
    }

    return 1;
}


// Выход
void SDL_app_shutdown(SDL_app_ctx* app)
{
    if (app->renderer)
        SDL_DestroyRenderer(app->renderer);

    if (app->window)
        SDL_DestroyWindow(app->window);

    TTF_Quit();
    SDL_Quit();
}

// Настройка консоли
void cyrillic_console_setup()
{
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
}


// =========================================================================================== REALIZATION
