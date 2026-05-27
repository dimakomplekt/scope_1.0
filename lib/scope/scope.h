// scope.h

#pragma once

// =========================================================================================== IMPORT

#include "../engine.h"

// =========================================================================================== IMPORT


// =========================================================================================== SCOPE STRUCT

typedef enum scope_mode
{

    SCOPE_MODE_SCROLL_TO_LEFT,
    SCOPE_MODE_N_PERIODS

} scope_mode_en;


typedef struct scope {

    int width;
    int height;

    int screen_width;
    int screen_height;

    scope_mode_en current_mode;

    int periods_to_display;


} scope_ctx;


