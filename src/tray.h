#ifndef TRAY_H
#define TRAY_H

#include "indicator.h"

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
    SingleTrayIcon icons[KEY_COUNT]; // 0=Num, 1=Caps, 2=Scroll
    int width;
    int height;
} TrayContext;

bool tray_init(TrayContext *tray, Display *d, int screen, const AppConfig *config);
void tray_destroy(TrayContext *tray);
void tray_update(TrayContext *tray, const AppConfig *config, const AppState *state);
void tray_handle_event(TrayContext *tray, const AppConfig *config, AppState *state, XEvent *ev,
                       bool *out_open_gui, bool *out_show_menu, int *out_menu_x, int *out_menu_y);

#endif // TRAY_H
