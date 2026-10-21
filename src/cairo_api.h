#ifndef CAIRO_API_H
#define CAIRO_API_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <X11/Xlib.h>

typedef void cairo_surface_t;
typedef void cairo_t;

typedef struct {
    double x_bearing;
    double y_bearing;
    double width;
    double height;
    double x_advance;
    double y_advance;
} cairo_text_extents_t;

typedef struct {
    void *handle;
    cairo_surface_t *(*xlib_surface_create)(Display *, Drawable, Visual *, int, int);
    cairo_t *(*create)(cairo_surface_t *);
    void (*destroy)(cairo_t *);
    void (*surface_destroy)(cairo_surface_t *);
    void (*select_font_face)(cairo_t *, const char *, int, int);
    void (*set_font_size)(cairo_t *, double);
    void (*set_antialias)(cairo_t *, int);
    void (*text_extents)(cairo_t *, const char *, cairo_text_extents_t *);
    void (*move_to)(cairo_t *, double, double);
    void (*line_to)(cairo_t *, double, double);
    void (*close_path)(cairo_t *);
    void (*new_sub_path)(cairo_t *);
    void (*arc)(cairo_t *, double, double, double, double, double);
    void (*rectangle)(cairo_t *, double, double, double, double);
    void (*set_source_rgb)(cairo_t *, double, double, double);
    void (*set_line_width)(cairo_t *, double);
    void (*fill)(cairo_t *);
    void (*fill_preserve)(cairo_t *);
    void (*stroke)(cairo_t *);
    void (*paint)(cairo_t *);
    void (*scale)(cairo_t *, double, double);
    void (*show_text)(cairo_t *, const char *);
    cairo_surface_t *(*image_surface_create)(int format, int width, int height);
    int (*surface_write_to_png)(cairo_surface_t *surface, const char *filename);
} CairoAPI;

static inline bool cairo_api_load(CairoAPI *api) {
    if (!api) return false;
    if (api->handle) return true;
    memset(api, 0, sizeof(*api));

    api->handle = dlopen("libcairo.so.2", RTLD_LAZY | RTLD_GLOBAL);
    if (!api->handle) return false;

    #define LOAD_SYM(name) api->name = dlsym(api->handle, "cairo_" #name); if (!api->name) return false
    LOAD_SYM(xlib_surface_create);
    LOAD_SYM(create);
    LOAD_SYM(destroy);
    LOAD_SYM(surface_destroy);
    LOAD_SYM(select_font_face);
    LOAD_SYM(set_font_size);
    LOAD_SYM(set_antialias);
    LOAD_SYM(text_extents);
    LOAD_SYM(move_to);
    LOAD_SYM(line_to);
    LOAD_SYM(close_path);
    LOAD_SYM(new_sub_path);
    LOAD_SYM(arc);
    LOAD_SYM(rectangle);
    LOAD_SYM(set_source_rgb);
    LOAD_SYM(set_line_width);
    LOAD_SYM(fill);
    LOAD_SYM(fill_preserve);
    LOAD_SYM(stroke);
    LOAD_SYM(paint);
    LOAD_SYM(scale);
    LOAD_SYM(show_text);
    LOAD_SYM(image_surface_create);
    LOAD_SYM(surface_write_to_png);
    #undef LOAD_SYM

    return true;
}

#endif // CAIRO_API_H
