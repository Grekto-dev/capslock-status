#ifndef TRAY_H
#define TRAY_H

#include "indicator.h"
#include "cairo_api.h"
#include "xapp_api.h"

typedef struct {
    Window window;
    Window parent;
    LockKeyType key;
    bool active;
    bool docked;
    int width;
    int height;
} SingleTrayIcon;

typedef struct {
    Display *display;
    int screen;
    Window tray_mgr;
    GC gc;
    XFontStruct *font;
    CairoAPI cairo;
    bool cairo_available;
    SingleTrayIcon app_icon;
    SingleTrayIcon icons[KEY_COUNT]; // 0=Num, 1=Caps, 2=Scroll
    int width;
    int height;

    // XApp StatusIcon support (allows dragging into modern collapsible trays)
    XAppAPI xapp;
    void *xapp_icon;                 // Main app icon
    void *xapp_icons[KEY_COUNT];     // 0=Num, 1=Caps, 2=Scroll
    bool has_xapp;
    bool action_open_gui;
    bool action_show_menu;
    int action_menu_x;
    int action_menu_y;
    char icon_paths[KEY_COUNT][2][512]; // [key][0=off, 1=on][path]

    // Tooltip window for tray indicators
    Window tooltip_win;
    bool tooltip_visible;
    int tooltip_key;
} TrayContext;

bool tray_init(TrayContext *tray, Display *d, int screen, const AppConfig *config);
void tray_destroy(TrayContext *tray);
void tray_update(TrayContext *tray, const AppConfig *config, const AppState *state);
void tray_iterate(TrayContext *tray);
void tray_handle_event(TrayContext *tray, const AppConfig *config, AppState *state, XEvent *ev,
                       bool *out_open_gui, bool *out_show_menu, int *out_menu_x, int *out_menu_y);

#endif // TRAY_H
