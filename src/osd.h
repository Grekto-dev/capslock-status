#ifndef OSD_H
#define OSD_H

#include "indicator.h"
#include <time.h>

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
    void (*select_font_face)(cairo_t *, const char *, int, int);
    void (*set_font_size)(cairo_t *, double);
    void (*set_antialias)(cairo_t *, int);
    void (*text_extents)(cairo_t *, const char *, cairo_text_extents_t *);
    void (*move_to)(cairo_t *, double, double);
    void (*set_source_rgb)(cairo_t *, double, double, double);
    void (*set_line_width)(cairo_t *, double);
    void (*rectangle)(cairo_t *, double, double, double, double);
    void (*fill)(cairo_t *);
    void (*stroke)(cairo_t *);
    void (*show_text)(cairo_t *, const char *);
    void (*destroy)(cairo_t *);
    void (*surface_destroy)(cairo_surface_t *);
} CairoAPI;

typedef struct {
    Display *display;
    int screen;
    Window window;
    Pixmap pixmap;
    GC gc;
    XFontSet font_title;
    XFontSet font_status;
    CairoAPI cairo;
    bool cairo_available;
    int width;
    int height;
    bool visible;
    struct timespec show_time;
    struct timespec hide_time;
    int total_duration_ms;
    float current_opacity;
    Atom atom_net_opacity;
    LockKeyType current_key;
    bool current_state;
} OSDContext;

bool osd_init(OSDContext *osd, Display *d, int screen, const AppConfig *config);
void osd_destroy(OSDContext *osd);
void osd_show(OSDContext *osd, const AppConfig *config, LockKeyType key, bool is_on);
void osd_hide(OSDContext *osd);
bool osd_is_visible(const OSDContext *osd);
long osd_get_remaining_ms(const OSDContext *osd);
void osd_update_animation(OSDContext *osd, const AppConfig *config);
void osd_set_opacity(OSDContext *osd, float opacity);
void osd_handle_event(OSDContext *osd, const AppConfig *config, XEvent *ev);
void osd_redraw(OSDContext *osd, const AppConfig *config);

#endif // OSD_H
