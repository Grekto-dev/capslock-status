#include "tray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#define SYSTEM_TRAY_REQUEST_DOCK 0

static void dock_window(Display *d, Window tray_mgr, Window win) {
    Atom opcode = XInternAtom(d, "_NET_SYSTEM_TRAY_OPCODE", False);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray_mgr;
    ev.xclient.message_type = opcode;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
    ev.xclient.data.l[2] = win;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;

    XSendEvent(d, tray_mgr, False, NoEventMask, &ev);
    XSync(d, False);
}

static const char *get_key_letter(LockKeyType key) {
    switch (key) {
        case KEY_NUM_LOCK:    return "N";
        case KEY_CAPS_LOCK:   return "C";
        case KEY_SCROLL_LOCK: return "S";
        default:              return "?";
    }
}

static const char *get_key_name(LockKeyType key) {
    switch (key) {
        case KEY_NUM_LOCK:    return "Num Lock";
        case KEY_CAPS_LOCK:   return "Caps Lock";
        case KEY_SCROLL_LOCK: return "Scroll Lock";
        default:              return "Lock";
    }
}

static bool is_icon_enabled(const AppConfig *config, LockKeyType key) {
    if (config->show_no_icons) return false;
    switch (key) {
        case KEY_NUM_LOCK:    return config->icon_num;
        case KEY_CAPS_LOCK:   return config->icon_caps;
        case KEY_SCROLL_LOCK: return config->icon_scroll;
        default:              return false;
    }
}

bool tray_init(TrayContext *tray, Display *d, int screen, const AppConfig *config) {
    if (!tray || !d) return false;
    memset(tray, 0, sizeof(*tray));

    tray->display = d;
    tray->screen = screen;
    tray->width = 22;
    tray->height = 22;

    char tray_atom_name[32];
    snprintf(tray_atom_name, sizeof(tray_atom_name), "_NET_SYSTEM_TRAY_S%d", screen);
    Atom tray_atom = XInternAtom(d, tray_atom_name, True);
    if (tray_atom == None) return false;

    tray->tray_mgr = XGetSelectionOwner(d, tray_atom);
    if (tray->tray_mgr == None) return false;

    Window root = RootWindow(d, screen);
    Atom xembed_atom = XInternAtom(d, "_XEMBED_INFO", False);
    long xembed_data[2] = {0, 1}; // version 0, XEMBED_MAPPED

    LockKeyType keys[KEY_COUNT] = {KEY_NUM_LOCK, KEY_CAPS_LOCK, KEY_SCROLL_LOCK};
    bool any_created = false;

    for (int i = 0; i < KEY_COUNT; i++) {
        LockKeyType k = keys[i];
        tray->icons[k].key = k;
        tray->icons[k].docked = false;
        tray->icons[k].parent = root;
        tray->icons[k].window = None;
        tray->icons[k].width = tray->width;
        tray->icons[k].height = tray->height;

        if (is_icon_enabled(config, k)) {
            // SAFE XEMBED:
            // 1. Create with override_redirect = True and offscreen (-200, -200)
            // 2. Set _XEMBED_INFO
            // 3. Send SYSTEM_TRAY_REQUEST_DOCK
            // 4. NEVER call XMapWindow or XMapRaised! The tray manager reparents and maps it.
            XSetWindowAttributes attrs;
            memset(&attrs, 0, sizeof(attrs));
            attrs.override_redirect = True;
            attrs.background_pixel = 0x1C1C1C;
            attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask;

            Window win = XCreateWindow(d, root,
                                       -200, -200, tray->width, tray->height,
                                       0, CopyFromParent, InputOutput,
                                       CopyFromParent,
                                       CWOverrideRedirect | CWBackPixel | CWEventMask,
                                       &attrs);
            if (win) {
                // Set _XEMBED_INFO
                XChangeProperty(d, win, xembed_atom, xembed_atom, 32,
                                PropModeReplace, (unsigned char *)xembed_data, 2);

                char title[64];
                snprintf(title, sizeof(title), "%s Indicator", get_key_name(k));
                XStoreName(d, win, title);

                dock_window(d, tray->tray_mgr, win);

                tray->icons[k].window = win;
                any_created = true;
            }
        }
    }

    if (any_created) {
        tray->gc = XCreateGC(d, root, 0, NULL);
        tray->font = XLoadQueryFont(d, "-*-*-bold-r-*-*-11-*-*-*-*-*-*-*");
        if (!tray->font) {
            tray->font = XLoadQueryFont(d, "fixed");
        }
        if (tray->font) {
            XSetFont(d, tray->gc, tray->font->fid);
        }
        return true;
    }

    return false;
}

void tray_destroy(TrayContext *tray) {
    if (!tray || !tray->display) return;

    if (tray->font) {
        XFreeFont(tray->display, tray->font);
        tray->font = NULL;
    }
    if (tray->gc) {
        XFreeGC(tray->display, tray->gc);
        tray->gc = NULL;
    }
    for (int i = 0; i < KEY_COUNT; i++) {
        if (tray->icons[i].window) {
            XDestroyWindow(tray->display, tray->icons[i].window);
            tray->icons[i].window = None;
        }
        tray->icons[i].docked = false;
    }
}

static void draw_single_icon(TrayContext *tray, const AppConfig *config, LockKeyType key, bool is_on) {
    Window win = tray->icons[key].window;
    if (!win || !tray->display || !tray->gc) return;

    Display *d = tray->display;
    GC gc = tray->gc;

    int w = tray->icons[key].width > 0 ? tray->icons[key].width : tray->width;
    int h = tray->icons[key].height > 0 ? tray->icons[key].height : tray->height;

    // Outer background matching panel
    XSetForeground(d, gc, 0x1C1C1C);
    XFillRectangle(d, win, gc, 0, 0, w, h);

    // Inner square: 16x16 centered in allocated tray size
    int bw = 16, bh = 16;
    int bx = (w - bw) / 2;
    int by = (h - bh) / 2;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    // Fill inner rectangle (#424242)
    XSetForeground(d, gc, config->bg_color);
    XFillRectangle(d, win, gc, bx, by, bw, bh);

    // Border: Green (#4DB434) if ON, Red (#B4344D) if OFF (exact replica of cli_preview.png)
    unsigned long border = is_on ? config->border_on : config->border_off;
    XSetForeground(d, gc, border);
    XDrawRectangle(d, win, gc, bx, by, bw - 1, bh - 1);

    // Centered letter: N, C, or S
    const char *letter = get_key_letter(key);
    XSetForeground(d, gc, config->text_color);
    if (tray->font) {
        int tw = XTextWidth(tray->font, letter, 1);
        int tx = bx + (bw - tw) / 2;
        int ty = by + (bh + tray->font->ascent - tray->font->descent) / 2;
        XDrawString(d, win, gc, tx, ty, letter, 1);
    } else {
        XDrawString(d, win, gc, bx + 5, by + 12, letter, 1);
    }

    // Tooltip update
    char tooltip[128];
    snprintf(tooltip, sizeof(tooltip), "%s: %s", get_key_name(key), is_on ? "ON" : "OFF");
    XStoreName(d, win, tooltip);

    XFlush(d);
}

void tray_update(TrayContext *tray, const AppConfig *config, const AppState *state) {
    if (!tray || !tray->display) return;

    for (int k = 0; k < KEY_COUNT; k++) {
        if (tray->icons[k].window != None) {
            bool is_on = state->state[k];
            // Smart tray: hide if tray_only_when_active and not active
            if (config->tray_only_when_active && !is_on) {
                if (tray->icons[k].docked) {
                    XUnmapWindow(tray->display, tray->icons[k].window);
                }
            } else {
                if (tray->icons[k].docked) {
                    XMapWindow(tray->display, tray->icons[k].window);
                    draw_single_icon(tray, config, (LockKeyType)k, is_on);
                }
            }
        }
    }
}

void tray_handle_event(TrayContext *tray, const AppConfig *config, AppState *state, XEvent *ev,
                       bool *out_open_gui, bool *out_show_menu, int *out_menu_x, int *out_menu_y) {
    if (!tray || !ev) return;

    Window root = RootWindow(tray->display, tray->screen);

    for (int k = 0; k < KEY_COUNT; k++) {
        if (tray->icons[k].window != None && ev->xany.window == tray->icons[k].window) {
            if (ev->type == Expose && ev->xexpose.count == 0) {
                draw_single_icon(tray, config, (LockKeyType)k, state->state[k]);
            } else if (ev->type == ReparentNotify) {
                if (ev->xreparent.parent != root) {
                    tray->icons[k].docked = true;
                    tray->icons[k].parent = ev->xreparent.parent;
                    draw_single_icon(tray, config, (LockKeyType)k, state->state[k]);
                } else {
                    tray->icons[k].docked = false;
                    tray->icons[k].parent = root;
                }
            } else if (ev->type == ConfigureNotify) {
                tray->icons[k].width = ev->xconfigure.width;
                tray->icons[k].height = ev->xconfigure.height;
                if (tray->icons[k].docked) {
                    draw_single_icon(tray, config, (LockKeyType)k, state->state[k]);
                }
            } else if (ev->type == ButtonPress || ev->type == ButtonRelease) {
                FILE *f = fopen("/tmp/capslock_click.log", "a");
                if (f) {
                    fprintf(f, "EVENT: type=%d button=%d x=%d y=%d\n", ev->type, ev->xbutton.button, ev->xbutton.x_root, ev->xbutton.y_root);
                    fclose(f);
                }
                if (ev->type == ButtonPress) {
                    if (ev->xbutton.button == 1) { // Left click: Open Settings GUI (Windows behavior)
                        if (out_open_gui) *out_open_gui = true;
                    } else if (ev->xbutton.button == 3) { // Right click: Open Context Menu (Windows behavior)
                        if (out_show_menu) {
                            *out_show_menu = true;
                            if (out_menu_x) *out_menu_x = ev->xbutton.x_root;
                            if (out_menu_y) *out_menu_y = ev->xbutton.y_root;
                        }
                    }
                }
            }
            break;
        }
    }
}
