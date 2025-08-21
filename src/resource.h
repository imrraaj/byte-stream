#ifndef RESOURCE_H
#define RESOURCE_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "raylib.h"
#include "bundle.h"

Texture2D bundle_load_texture(const char *file_path);
Font bundle_load_font(const char *file_path, int font_size);
void *bundle_load_resource(const char *file_path, size_t *size);

#endif // RESOURCE_H
