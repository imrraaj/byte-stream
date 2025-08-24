#ifndef APPLICATION_H
#define APPLICATION_H
#include <assert.h>
#include <stdlib.h>

#include "font.h"
#include "macros.h"
#include "raylib.h"
#include "resource.h"

#define BACKGROUND_COLOR GetColor(0x181920FF)
#define SECONDARY_BGCOLOR GetColor(0x343434FF)
// #define ACCENT_COLOR GetColor(0x133E31FF)
#define ACCENT_COLOR RAYWHITE
#define UI_FONT "./assets/fonts/SourceSans3-Bold.ttf"
#define SUBTITLE_FONT "./assets/fonts/GoogleSansText-Bold.ttf"

typedef struct {
    Custom_Fonts fonts;
} Application;

void init_application(Application *app);
void cleanup_application(Application *app);
#endif // APPLICATION_H
