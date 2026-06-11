
// my_sdl_textbox.c

// =========================================================================================== IMPORT

#include "my_sdl_textbox.h"

#include <math.h>
#include <windows.h>

// =========================================================================================== IMPORT



Textbox* Textbox_init(SDL_Color color, int font_size)
{
    
    Textbox* textbox = malloc(sizeof(Textbox));

    if (!textbox)
        return NULL;


    memset(textbox, 0, sizeof(Textbox));

    textbox->x = 100;
    textbox->y = 100;


    textbox->color = color;
    textbox->font_size = font_size;

    char font_path[MAX_PATH];

    absolute_by_relative_from_exe(
        "content/fonts/basis33.ttf",
        font_path,
        sizeof(font_path)
    );

    textbox->font = TTF_OpenFont(font_path, font_size);

    if (!textbox->font)
    {
        free(textbox);
        return NULL;
    }

    strcpy_s(textbox->content,
             sizeof(textbox->content),
             "Text");

    textbox->dirty = true;

    textbox->draw_mode = HORIZONTAL_TEXT;

    return textbox;
}

void Textbox_set_content(Textbox* textbox, const char* text)
{
    if (!textbox || !text) return;


    strcpy_s(
        textbox->content,
        sizeof(textbox->content),
        text
    );

    textbox->dirty = true;
}


void Textbox_update(Textbox* textbox, SDL_Renderer* renderer)
{
    if (!textbox->dirty)
        return;

    if (textbox->texture)
    {
        SDL_DestroyTexture(textbox->texture);
        textbox->texture = NULL;
    }

    SDL_Surface* surface =
        TTF_RenderText_Blended(
            textbox->font,
            textbox->content,
            textbox->color
        );

    if (!surface)
        return;

    textbox->w = surface->w;
    textbox->h = surface->h;

    textbox->texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface
        );

    SDL_FreeSurface(surface);

    textbox->dirty = false;
}

void Textbox_render(Textbox* textbox, SDL_Renderer* renderer)
{
    if (!textbox->texture)
        return;

    SDL_Rect dst;

    dst.w = textbox->w;
    dst.h = textbox->h;

    dst.x = textbox->x - dst.w / 2;
    dst.y = textbox->y - dst.h / 2;

    double angle = 0.0;

    if (textbox->draw_mode == VERTICAL_TEXT)
        angle = 90.0;

    SDL_RenderCopyEx(
        renderer,
        textbox->texture,
        NULL,
        &dst,
        angle,
        NULL,
        SDL_FLIP_NONE
    );
}


void Textbox_set_draw_mode(
    Textbox* textbox,
    text_draw_mode new_draw_mode
)
{
    if (!textbox)
        return;

    textbox->draw_mode = new_draw_mode;
}


void get_exe_dir(char* out_path, size_t size)
{
    GetModuleFileNameA(NULL, out_path, (DWORD)size);

    char* last_slash = strrchr(out_path, '\\');

    if (last_slash)
    {
        *last_slash = '\0';
    }
}


void Textbox_destroy(Textbox* textbox)
{
    if (!textbox)
        return;

    if (textbox->texture)
        SDL_DestroyTexture(textbox->texture);

    if (textbox->font)
        TTF_CloseFont(textbox->font);

    free(textbox);
}


/*

char font_path[MAX_PATH];

absolute_by_relative_from_exe(

    "content\\fonts\\basis33.ttf",
    font_path,
    sizeof(font_path)
    
);

printf("%s\n", font_path);


*/
void absolute_by_relative_from_exe(

    const char* relative_path,
    char* out_path,
    size_t size

)
{
    get_exe_dir(out_path, size);

    strcat_s(out_path, size, "\\");
    strcat_s(out_path, size, relative_path);
}