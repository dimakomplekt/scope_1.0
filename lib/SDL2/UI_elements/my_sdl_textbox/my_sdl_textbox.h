// my_sdl_textbox.h

#pragma once

// =========================================================================================== IMPORT

#include <stdbool.h>

#include "../../my_sdl_draw/my_sdl_draw.h"
#include "../../global_inputs/global_inputs.h"


#include <stdlib.h>
#include <string.h>


// =========================================================================================== IMPORT


// =========================================================================================== TEXTBOX


typedef struct Textbox
{
    // положение центра
    int x;
    int y;

    // размеры текста
    int w;
    int h;

    // настройки
    SDL_Color color;
    int font_size;

    // данные
    char content[256];

    // ttf
    TTF_Font* font;

    // кэш
    SDL_Texture* texture;
    bool dirty;

};



// =========================================================================================== API

// Функция инициализации, нельзя сделать напрямую генерацию шрифта по адресу - нужно в рантайме коллить создание шрифта
// или делать систему сложнее, что неуместно в данной задаче
Textbox* Textbox_init(SDL_Color color, int font_size);

void Textbox_set_content(Textbox* textbox, const char* text);


void Textbox_update(Textbox* textbox, SDL_Renderer* renderer);

void Textbox_render(Textbox* textbox, SDL_Renderer* renderer);

void Textbox_destroy(Textbox* textbox);

void absolute_by_relative_from_exe(

    const char* relative_path,
    char* out_path,
    size_t size

);