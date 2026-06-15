// app.c

// =========================================================================================== IMPORT

#include "app.h"
#include <stdio.h>


#include "../lib/app_timer/app_timer.h"
#include "../lib/sin_generator/sin_generator.h"


// =========================================================================================== IMPORT

// =========================================================================================== DATA

Scope scope_1;

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
    
    printf("blend mode = %d\n", mode);

    app->running = 1;
    app->name = title;

    GI_init();


    scope_init(&scope_1, app->renderer);

    sin_generator_init(5.0, 300.0);
    
    signal_check(&scope_1, &Oscillator_1);

    printf("Scope signal A: .%f", scope_1.signal_control_data.controlled_signal->amplitude);
    printf("Scope signal F: .%f", scope_1.signal_control_data.controlled_signal->frequency);

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

double curr_time;

// Апдейт апы в котором идут апдейты всех объектов
void SDL_app_update(SDL_app_ctx* app)
{
    app_timer_update();

    double curr_time = app_timer_get_time();

    if (start_time_1 == 0) start_time_1 = curr_time;


    sin_generator_update();

    scope_buffer_update(&scope_1);


    const double STEP_240 = 1.0 / 240.0;

    while (curr_time - start_time_1 >= STEP_240)
    {
        GI_update();

        buffer_analysis(&scope_1);

        scope_update(&scope_1);

        printf("Scope signal value: .%f \n", sin_generator_get_clean());
        printf("Scope signal value from buffer: .%f \n", scope_1.signal_control_data.scope_buffer_data.samples[scope_1.signal_control_data.scope_buffer_data.head - 1].value);
        printf("Scope signal by analysis A: .%f \n", scope_1.signal_control_data.current_max_signal_value);
        printf("Scope signal by analysis F: .%f \n\n", scope_1.signal_control_data.current_period_value);


        start_time_1 += STEP_240;
    }
}


// Рендер апы в котором идут рендеры всех объектов
void SDL_app_render(SDL_app_ctx* app)
{
    double curr_time = app_timer_get_time();

    if (start_time_2 == 0) start_time_2 = curr_time;

    const double STEP_60 = 1.0 / 60.0;

    while (curr_time - start_time_2 >= STEP_60)
    {
        SDL_SetRenderDrawColor(app->renderer, 10, 10, 10, 255);
        SDL_RenderClear(app->renderer);

        scope_render(&scope_1);

        printf("Scope signal Tpx from render: .%f", scope_1.scope_render_data.signal_render_data.points[1].x);
        printf("Scope signal Vpx from render: .%f", scope_1.scope_render_data.signal_render_data.points[1].y);
        printf("Scope signal show:  .%s\n", scope_1.scope_render_data.signal_render_data.points[1].show ? "true" : "false");



        SDL_RenderPresent(app->renderer);

        start_time_2 += STEP_60;
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

        SDL_Delay(1);
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
