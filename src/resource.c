#include "resource.h"
void *bundle_load_resource(const char *file_path, size_t *size) {
    for (size_t i = 0; i < resources_count; ++i) {
        if (strcmp(resources[i].file_path, file_path) == 0) {
            *size = resources[i].size;
            return &bundle[resources[i].offset];
        }
    }
    return NULL;
}

Font bundle_load_font(const char *file_path, int font_size) {
    size_t data_size;
    void *data = bundle_load_resource(file_path, &data_size);
    int codepoint_count = 95;
    int *codepoints = LoadCodepoints(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", &codepoint_count);
    Font output = LoadFontFromMemory(GetFileExtension(file_path), data, data_size, font_size, codepoints, codepoint_count);
    UnloadCodepoints(codepoints);
    return output;
}

Texture bundle_load_texture(const char *file_path) {
    size_t data_size;
    void *data = bundle_load_resource(file_path, &data_size);
    Image image = LoadImageFromMemory(GetFileExtension(file_path), data, data_size);
    Texture output = LoadTextureFromImage(image);
    return output;
}
