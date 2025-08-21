#ifndef FONT_H
#define FONT_H
#include "raylib.h"
#include "resource.h"
#include "macros.h"
#include <stdlib.h>

typedef struct
{
    Font font;
    int size;
    const char *name;
} Custom_Font;

typedef struct
{
    Custom_Font *items;
    size_t capacity;
    size_t count;
} Custom_Fonts;

Custom_Fonts init_fonts(void);
void cleanup_fonts(Custom_Fonts fonts);
Font get_best_font(Custom_Fonts fonts, int target, const char *name);
#endif // FONT_H
