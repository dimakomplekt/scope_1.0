// my_sdl_draw.c


// =========================================================================================== IMPORT

#include "my_sdl_draw.h"

#include <math.h>

// =========================================================================================== IMPORT


// =========================================================================================== Drawing functions

static unsigned char hex_pair_to_byte(const char* str)
{
    char buf[3];
    buf[0] = str[0];
    buf[1] = str[1];
    buf[2] = '\0';

    return (unsigned char)strtol(buf, NULL, 16);
}

SDL_Color hex_to_sdl_color(const char* hex, Uint8 opacity)
{
    SDL_Color color = {0, 0, 0, opacity};

    if (!hex)
        return color;

    // ожидаем формат "#RRGGBB"
    if (hex[0] == '#')
        hex++;

    color.r = hex_pair_to_byte(hex);
    color.g = hex_pair_to_byte(hex + 2);
    color.b = hex_pair_to_byte(hex + 4);
    color.a = opacity;

    return color;
}


void my_sdl_draw_pixel(
    
    SDL_Renderer* renderer,
    int x,
    int y,
    SDL_Color color
    
)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawPoint(renderer, x, y);
}


void my_sdl_draw_line(

    SDL_Renderer* renderer,
    int x1, int y1,
    int x2, int y2,
    int thickness,
    SDL_Color color

)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r,
        color.g,
        color.b,
        color.a
    );

    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);

    float len = sqrtf(dx * dx + dy * dy);

    if (len == 0.0f)
        return;

    dx /= len;
    dy /= len;

    float nx = -dy;
    float ny = dx;

    float start = -(thickness - 1) * 0.5f;

    for (int i = 0; i < thickness; i++)
    {
        float offset = start + i;

        SDL_RenderDrawLine(
            renderer,
            (int)(x1 + nx * offset),
            (int)(y1 + ny * offset),
            (int)(x2 + nx * offset),
            (int)(y2 + ny * offset)
        );
    }
}


void my_sdl_draw_rect(

    SDL_Renderer* renderer,
    int x, int y,
    int w, int h,
    int thickness,
    SDL_Color color

)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    x -= w / 2;
    y -= h / 2;

    for (int i = 0; i < thickness; i++)
    {
        SDL_Rect r = {
            x + i,
            y + i,
            w - 2 * i,
            h - 2 * i
        };

        SDL_RenderDrawRect(renderer, &r);
    }
}


void my_sdl_draw_filled_rect_bi(

    SDL_Renderer* renderer,
    int x, int y,
    int w, int h,
    SDL_Color fill_color,
    int border_thickness,
    SDL_Color border_color

)
{
    x -= w / 2;
    y -= h / 2;

    // FILL

    SDL_SetRenderDrawColor(
        renderer,
        fill_color.r,
        fill_color.g,
        fill_color.b,
        fill_color.a
    );

    SDL_Rect fill = { x, y, w, h };
    SDL_RenderFillRect(renderer, &fill);


    // BORDER INSIDE

    if (border_thickness > 0)
    {
        SDL_SetRenderDrawColor(
            renderer,
            border_color.r,
            border_color.g,
            border_color.b,
            border_color.a
        );

        for (int i = 0; i < border_thickness; i++)
        {
            SDL_Rect border =
            {
                x + i,
                y + i,
                w - 2 * i,
                h - 2 * i
            };

            SDL_RenderDrawRect(renderer, &border);
        }
    }
}

void my_sdl_draw_filled_circle(

    SDL_Renderer* renderer,
    int cx,
    int cy,
    int radius,
    SDL_Color color
    
)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            if (x*x + y*y <= radius*radius)
            {
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}


// =========================================================================================== Drawing functions