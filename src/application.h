#ifndef APPLICATION_H
#define APPLICATION_H
#include "raylib.h"
#include "font.h"
#include "resource.h"
#include "macros.h"
#include <stdlib.h>
#include <assert.h>



#define BACKGROUND_COLOR GetColor(0x181920FF)
#define SECONDARY_BGCOLOR GetColor(0x343434FF)
#define ACCENT_COLOR GetColor(0x133E31FF)

typedef struct {
    Custom_Fonts fonts;
} Application;


void init_application(Application *app);
void cleanup_application(Application *app);
#endif // APPLICATION_H
