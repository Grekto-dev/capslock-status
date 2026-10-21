#ifndef OSD_H
#define OSD_H

#include "indicator.h"
#include "cairo_api.h"
#include <time.h>

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
