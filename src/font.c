#include "font.h"

Font get_best_font(Custom_Fonts fonts, int target)
{
    for (size_t i = 0; i < fonts.count; i++)
    {
        if (fonts.items[i].size == target)
        {
            return fonts.items[i].font;
        }
    }
    return fonts.items[0].font;
}

Custom_Fonts init_fonts(void)
{
    Custom_Fonts fonts = {0};
    int font_sizes[] = {12, 16, 18, 20, 24, 28, 30, 32, 36, 48, 64, 72};
    for (size_t i = 0; i < ARRAY_LEN(font_sizes); i++)
    {
        Custom_Font f = {
            .font = bundle_load_font("./assets/fonts/CircularSpotifyText-Bold.otf", font_sizes[i]),
            .size = font_sizes[i]};
        da_append(fonts, f);
    }
    return fonts;
}

void cleanup_fonts(Custom_Fonts fonts)
{
    for (size_t i = 0; i < fonts.count; i++)
    {
        UnloadFont(fonts.items[i].font);
    }
    if (fonts.items)
    {
        free(fonts.items);
        fonts.items = NULL;
        fonts.count = 0;
        fonts.capacity = 0;
    }
}
