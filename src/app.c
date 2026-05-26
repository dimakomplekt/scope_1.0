// app.c

// =========================================================================================== IMPORT

#include "app.h"

// =========================================================================================== IMPORT


// =========================================================================================== REALIZATION

int SDL_app_init_and_run()
{
    SDL_app_ctx app = {0};

    if (!SDL_app_init(&app, SCREEN_WIDTH, SCREEN_HEIGHT, "Scope_1.0")) return -1;

    SDL_app_run(&app);

    SDL_app_shutdown(&app);

    return 0;
}


int SDL_app_init(SDL_app_ctx* app, int w, int h, const char* title)
{

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return 0;

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

    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);

    app->running = 1;
    app->name = title;


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


// Апдейт апы в котором идут апдейты всех объектов
void SDL_app_update(SDL_app_ctx* app)
{
    // ===== UPDATE =====


    // ===== UPDATE =====
}


// Рендер апы в котором идут рендеры всех объектов
void SDL_app_render(SDL_app_ctx* app)
{
    SDL_SetRenderDrawColor(app->renderer, 10, 10, 10, 255);
    SDL_RenderClear(app->renderer);

    // ===== RENDER =====

    // =========================
    // TEST RECTANGLE
    // =========================

    my_sdl_draw_filled_rect(

    app->renderer,
    SCREEN_WIDTH / 2,
    SCREEN_HEIGHT / 2,
    200,
    200,
    hex_to_sdl_color("#FF0000", 255),
    hex_to_sdl_color("#7bec03", 255),
    5

    );
    
    // =========================

    // ===== RENDER =====

    SDL_RenderPresent(app->renderer);
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
