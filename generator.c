#include <stdbool.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"
#define genf(out, ...) \
    do { \
        fprintf((out), __VA_ARGS__); \
        fprintf((out), " // %s:%d\n", __FILE__, __LINE__); \
    } while(0)

typedef struct {
  const char *file_path;
  size_t offset;
  size_t size;
} Resource;

Resource resources[] = {
    { .file_path = "./assets/icons/bb.png" },
    { .file_path = "./assets/icons/ff.png" },
    { .file_path = "./assets/icons/pause.png" },
    { .file_path = "./assets/icons/play.png" },
    { .file_path = "./assets/icons/stop.png" },
    { .file_path = "./assets/fonts/CircularSpotifyText-Black.otf" },
    { .file_path = "./assets/fonts/CircularSpotifyText-Bold.otf" },
    {.file_path = "./assets/logos/bytestream-256.png" },
};

bool generate_resource_bundle(void)
{
  bool result = true;
  Nob_String_Builder bundle = {0};
  Nob_String_Builder content = {0};
  FILE *out = NULL;

  // bundle  = [aaaaaaaaabbbbb]
  //            ^        ^
  // content = []
  // 0, 9

  for (size_t i = 0; i < NOB_ARRAY_LEN(resources); ++i) {
    content.count = 0;
        if (!nob_read_entire_file(resources[i].file_path, &content)) nob_return_defer(false);
    resources[i].offset = bundle.count;
    resources[i].size = content.count;
    nob_da_append_many(&bundle, content.items, content.count);
    nob_da_append(&bundle, 0);
  }

  const char *bundle_h_path = "./include/bundle.h";
  out = fopen(bundle_h_path, "wb");
  if (out == NULL) {
        nob_log(NOB_ERROR, "Could not open file %s for writing: %s", bundle_h_path, strerror(errno));
    nob_return_defer(false);
  }

  genf(out, "#ifndef BUNDLE_H_");
  genf(out, "#define BUNDLE_H_");
  genf(out, "typedef struct {");
  genf(out, "    const char *file_path;");
  genf(out, "    size_t offset;");
  genf(out, "    size_t size;");
  genf(out, "} Resource;");
  genf(out, "size_t resources_count = %zu;", NOB_ARRAY_LEN(resources));
  genf(out, "Resource resources[] = {");
  for (size_t i = 0; i < NOB_ARRAY_LEN(resources); ++i) {
    genf(out, "    {.file_path = \"%s\", .offset = %zu, .size = %zu},",
         resources[i].file_path, resources[i].offset, resources[i].size);
  }
  genf(out, "};");

  genf(out, "unsigned char bundle[] = {");
  size_t row_size = 20;
    for (size_t i = 0; i < bundle.count; ) {
    fprintf(out, "     ");
    for (size_t col = 0; col < row_size && i < bundle.count; ++col, ++i) {
      fprintf(out, "0x%02X, ", (unsigned char)bundle.items[i]);
    }
    genf(out, "");
  }
  genf(out, "};");
  genf(out, "#endif // BUNDLE_H_");

  nob_log(NOB_INFO, "Generated %s", bundle_h_path);

defer:
    if (out) fclose(out);
  free(content.items);
  free(bundle.items);
  return result;
}

int main(int argc, char **argv)
{
  NOB_GO_REBUILD_URSELF(argc, argv);
  nob_log(NOB_INFO, "--- Bundling Resources ---");
    if (!generate_resource_bundle()) return 1;
  return 0;
}
