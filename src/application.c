#include "application.h"
#include "font.h"
#include <assert.h>

void init_application(Application *app) {
    assert(app != NULL);
    app->fonts = init_fonts();
}

void cleanup_application(Application *app) {
    assert(app != NULL);
    cleanup_fonts(app->fonts);
    app->fonts.items = NULL;
    app->fonts.count = 0;
}
