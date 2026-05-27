
// my_sdl_button.c

// =========================================================================================== IMPORT

#include "my_sdl_button.h"

#include <math.h>

// =========================================================================================== IMPORT


// =========================================================================================== INTERNAL

static bool point_inside(Button* btn, int px, int py)
{
    // =======================================================================================
    // CIRCLE
    // =======================================================================================

    if (btn->radius >= btn->w / 2 &&
        btn->radius >= btn->h / 2)
    {
        int cx = btn->x;
        int cy = btn->y;

        int r = btn->w / 2;

        int dx = px - cx;
        int dy = py - cy;

        return (dx * dx + dy * dy) <= (r * r);
    }

    // =======================================================================================
    // RECTANGLE / ROUNDED RECTANGLE
    // =======================================================================================

    int left   = btn->x - btn->w / 2;
    int right  = btn->x + btn->w / 2;

    int top    = btn->y - btn->h / 2;
    int bottom = btn->y + btn->h / 2;

    return
        px >= left
        &&
        px <= right
        &&
        py >= top
        &&
        py <= bottom;
}


// =========================================================================================== UPDATE

void BTN_update(Button* btn)
{
    int mx = (int)GI_mouse_x();
    int my = (int)GI_mouse_y();

    bool hovered = point_inside(btn, mx, my);

    // НАЖАЛИ
    if (hovered && GI_lb_pressed())
    {
        btn->down_inside = true;
    }

    // ОТПУСТИЛИ
    if (GI_lb_just_released())
    {
        if (btn->down_inside && hovered && btn->on_click)
        {
            btn->on_click(btn);
        }

        btn->down_inside = false;
    }

    btn->hovered = hovered;
}


// =========================================================================================== RENDER

void BTN_render(Button* btn, SDL_Renderer* renderer)
{
    SDL_Color color;

    if (btn->down_inside)
        color = btn->pressed_color;
    else if (btn->hovered)
        color = btn->hover_color;
    else
        color = btn->idle_color;

    // =========================
    // SHAPE
    // =========================

    if (btn->radius <= 0)
    {
        my_sdl_draw_filled_rect(
            renderer,
            btn->x,
            btn->y,
            btn->w,
            btn->h,
            color,
            color,
            0
        );


        // =========================
        // BORDER (пока прямоугольный)
        // =========================

        if (btn->border_thickness > 0)
        {
            my_sdl_draw_rect(
                renderer,
                btn->x,
                btn->y,
                btn->w,
                btn->h,
                btn->border_color,
                btn->border_thickness
            );
        }
    }
    else if (btn->radius >= btn->w / 2 && btn->radius >= btn->h / 2)
    {
        int outer_r = btn->w / 2;
        int inner_r = outer_r - btn->border_thickness;
    
        if (inner_r < 0) inner_r = 0;
    
        // =========================
        // BORDER (outer circle)
        // =========================
        my_sdl_draw_filled_circle(
            renderer,
            btn->x,
            btn->y,
            outer_r,
            btn->border_color
        );
    
        // =========================
        // FILL (inner circle)
        // =========================
        my_sdl_draw_filled_circle(
            renderer,
            btn->x,
            btn->y,
            inner_r,
            color
        );
    }
    else
    {
        // fallback — прямоугольник
        my_sdl_draw_filled_rect(
            renderer,
            btn->x,
            btn->y,
            btn->w,
            btn->h,
            color,
            color,
            0
        );


        // =========================
        // BORDER (пока прямоугольный)
        // =========================

        if (btn->border_thickness > 0)
        {
            my_sdl_draw_rect(
                renderer,
                btn->x,
                btn->y,
                btn->w,
                btn->h,
                btn->border_color,
                btn->border_thickness
            );
        }
    }
}
