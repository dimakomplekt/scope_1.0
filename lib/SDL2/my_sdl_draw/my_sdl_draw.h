// my_sdl_draw.h

#pragma once

// =========================================================================================== IMPORT

#include "../../engine.h"

// =========================================================================================== IMPORT


// =========================================================================================== Drawing APIs

SDL_Color hex_to_sdl_color(const char* hex, Uint8 opacity);


void my_sdl_draw_pixel(
    
    SDL_Renderer* renderer,
    int x,
    int y,
    SDL_Color color
    
);


void my_sdl_draw_line(
    
    SDL_Renderer* renderer,
    int x1, int y1,
    int x2, int y2,
    SDL_Color color
                    
);


void my_sdl_draw_rect(

    SDL_Renderer* renderer,
    int x, int y,
    int w, int h,
    SDL_Color color,
    int thickness

);


void my_sdl_draw_filled_rect(

    SDL_Renderer* renderer,
    int x, int y,
    int w, int h,
    SDL_Color fill_color,
    SDL_Color border_color,
    int border_thickness

);


void my_sdl_draw_filled_circle(

    SDL_Renderer* renderer,
    int cx,
    int cy,
    int radius,
    SDL_Color color
    
);

// =========================================================================================== Drawing APIs