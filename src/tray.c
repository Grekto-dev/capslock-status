#include "tray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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

static void ensure_icon_cache_dir(char *out_dir, size_t max_len) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(out_dir, max_len, "%s/.cache/capslock-status/icons", home);

    char path[512];
    snprintf(path, sizeof(path), "%s/.cache", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.cache/capslock-status", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.cache/capslock-status/icons", home);
    mkdir(path, 0755);
}

static void generate_indicator_png(TrayContext *tray, const AppConfig *config, LockKeyType key, bool is_on, const char *filepath) {
    if (!tray->cairo_available || !tray->cairo.image_surface_create || !tray->cairo.surface_write_to_png) return;

    int size = 48; // Crisp 48x48 icon suitable for both 24px and 48px panels
    cairo_surface_t *surf = tray->cairo.image_surface_create(0, size, size); // 0 = CAIRO_FORMAT_ARGB32
    if (!surf) return;

    cairo_t *cr = tray->cairo.create(surf);
    if (!cr) {
        tray->cairo.surface_destroy(surf);
        return;
    }

    // Outer background: transparent
    // Inner box: 40x40 centered at (4, 4) with rounded corners
    double x = 4.0, y = 4.0, w = 40.0, h = 40.0, r = 4.0;

    tray->cairo.new_sub_path(cr);
    tray->cairo.arc(cr, x + w - r, y + r, r, -1.570796, 0);
    tray->cairo.arc(cr, x + w - r, y + h - r, r, 0, 1.570796);
    tray->cairo.arc(cr, x + r, y + h - r, r, 1.570796, 3.14159);
    tray->cairo.arc(cr, x + r, y + r, r, 3.14159, 4.71239);
    tray->cairo.close_path(cr);

    // Fill inner rectangle with config->bg_color
    unsigned long bg = config->bg_color;
    tray->cairo.set_source_rgb(cr, ((bg >> 16) & 0xFF) / 255.0,
                                   ((bg >> 8) & 0xFF) / 255.0,
                                   (bg & 0xFF) / 255.0);
    tray->cairo.fill_preserve(cr);

    // Border: config->border_on or config->border_off
    unsigned long border = is_on ? config->border_on : config->border_off;
    tray->cairo.set_source_rgb(cr, ((border >> 16) & 0xFF) / 255.0,
                                   ((border >> 8) & 0xFF) / 255.0,
                                   (border & 0xFF) / 255.0);
    tray->cairo.set_line_width(cr, 3.0);
    tray->cairo.stroke(cr);

    // Letter: N, C, or S
    const char *letter = get_key_letter(key);
    unsigned long fg = config->text_color;
    tray->cairo.set_source_rgb(cr, ((fg >> 16) & 0xFF) / 255.0,
                                   ((fg >> 8) & 0xFF) / 255.0,
                                   (fg & 0xFF) / 255.0);
    tray->cairo.select_font_face(cr, "Sans", 0, 1); // slant normal, weight bold
    tray->cairo.set_font_size(cr, 26.0);

    cairo_text_extents_t ext;
    tray->cairo.text_extents(cr, letter, &ext);
    double tx = x + (w - ext.width) / 2.0 - ext.x_bearing;
    double ty = y + (h - ext.height) / 2.0 - ext.y_bearing;
    tray->cairo.move_to(cr, tx, ty);
    tray->cairo.show_text(cr, letter);

    tray->cairo.surface_write_to_png(surf, filepath);
    tray->cairo.destroy(cr);
    tray->cairo.surface_destroy(surf);
}

static void on_xapp_button_press(void *icon, int x, int y, unsigned int button, unsigned int time, int orientation, void *user_data) {
    (void)icon;
    (void)time;
    (void)orientation;
    TrayContext *tray = (TrayContext *)user_data;
    if (!tray) return;

    if (button == 1) {
        tray->action_open_gui = true;
    } else if (button == 3) {
        tray->action_show_menu = true;
        tray->action_menu_x = x;
        tray->action_menu_y = y;
        if (tray->action_menu_x <= 0 && tray->action_menu_y <= 0 && tray->display) {
            Window root = RootWindow(tray->display, tray->screen);
            Window root_ret, child_ret;
            int rx, ry, wx, wy;
            unsigned int mask;
            if (XQueryPointer(tray->display, root, &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask)) {
                tray->action_menu_x = rx;
                tray->action_menu_y = ry;
            }
        }
    }
}

void tray_iterate(TrayContext *tray) {
    if (!tray || !tray->has_xapp) return;
    while (tray->xapp.g_main_context_iteration(NULL, false)) {
        // Drain pending GLib / DBus events
    }
}

static void get_tooltip_size(TrayContext *tray, const char *text, int *out_w, int *out_h) {
    int pad_x = 10;
    int pad_y = 6;
    int tw = 60, th = 14;

    if (tray->cairo_available) {
        cairo_surface_t *surf = tray->cairo.xlib_surface_create(tray->display, RootWindow(tray->display, tray->screen),
                                                                 DefaultVisual(tray->display, tray->screen), 1, 1);
        if (surf) {
            cairo_t *cr = tray->cairo.create(surf);
            if (cr) {
                tray->cairo.select_font_face(cr, "Sans", 0, 0);
                tray->cairo.set_font_size(cr, 11.0);
                cairo_text_extents_t ext;
                tray->cairo.text_extents(cr, text, &ext);
                tw = (int)(ext.width + 0.5);
                th = (int)(ext.height + 0.5);
                tray->cairo.destroy(cr);
            }
            tray->cairo.surface_destroy(surf);
        }
    } else if (tray->font) {
        tw = XTextWidth(tray->font, text, (int)strlen(text));
        th = tray->font->ascent + tray->font->descent;
    }
    *out_w = tw + pad_x * 2;
    *out_h = th + pad_y * 2;
    if (*out_h < 22) *out_h = 22;
}

static void draw_tooltip(TrayContext *tray, const char *text, int width, int height) {
    if (!tray || !tray->tooltip_win || !tray->display) return;
    Display *d = tray->display;
    Window win = tray->tooltip_win;

    if (tray->cairo_available) {
        Visual *visual = DefaultVisual(d, tray->screen);
        cairo_surface_t *surf = tray->cairo.xlib_surface_create(d, win, visual, width, height);
        if (surf) {
            cairo_t *cr = tray->cairo.create(surf);
            if (cr) {
                // Background rounded box (#24272B)
                double x = 0.5, y = 0.5;
                double w = width - 1.0, h = height - 1.0;
                double r = 4.0;

                tray->cairo.new_sub_path(cr);
                tray->cairo.arc(cr, x + w - r, y + r, r, -1.570796, 0);
                tray->cairo.arc(cr, x + w - r, y + h - r, r, 0, 1.570796);
                tray->cairo.arc(cr, x + r, y + h - r, r, 1.570796, 3.14159);
                tray->cairo.arc(cr, x + r, y + r, r, 3.14159, 4.71239);
                tray->cairo.close_path(cr);

                tray->cairo.set_source_rgb(cr, 0x24 / 255.0, 0x27 / 255.0, 0x2B / 255.0);
                tray->cairo.fill_preserve(cr);

                // Border (#484D56)
                tray->cairo.set_source_rgb(cr, 0x48 / 255.0, 0x4D / 255.0, 0x56 / 255.0);
                tray->cairo.set_line_width(cr, 1.0);
                tray->cairo.stroke(cr);

                // Text (White #FFFFFF, Sans font)
                tray->cairo.select_font_face(cr, "Sans", 0, 0);
                tray->cairo.set_font_size(cr, 11.0);
                cairo_text_extents_t ext;
                tray->cairo.text_extents(cr, text, &ext);

                double tx = (width - ext.width) / 2.0 - ext.x_bearing;
                double ty = (height - ext.height) / 2.0 - ext.y_bearing;
                tray->cairo.set_source_rgb(cr, 1.0, 1.0, 1.0);
                tray->cairo.move_to(cr, tx, ty);
                tray->cairo.show_text(cr, text);

                tray->cairo.destroy(cr);
            }
            tray->cairo.surface_destroy(surf);
        }
    } else {
        GC gc = tray->gc;
        XSetForeground(d, gc, 0x24272B);
        XFillRectangle(d, win, gc, 0, 0, width, height);
        XSetForeground(d, gc, 0x484D56);
        XDrawRectangle(d, win, gc, 0, 0, width - 1, height - 1);
        XSetForeground(d, gc, 0xFFFFFF);
        if (tray->font) {
            int tw = XTextWidth(tray->font, text, (int)strlen(text));
            int tx = (width - tw) / 2;
            int ty = (height + tray->font->ascent - tray->font->descent) / 2;
            XDrawString(d, win, gc, tx, ty, text, (int)strlen(text));
        } else {
            XDrawString(d, win, gc, 8, height / 2 + 4, text, (int)strlen(text));
        }
    }
    XFlush(d);
}

static void tray_show_tooltip(TrayContext *tray, LockKeyType key, Window src_win) {
    if (!tray || !tray->display || !tray->tooltip_win || src_win == None) return;

    const char *text = (key == (LockKeyType)-1) ? "CapsLock Status" : get_key_name(key);
    int w = 0, h = 0;
    get_tooltip_size(tray, text, &w, &h);

    Display *d = tray->display;
    Window root = RootWindow(d, tray->screen);
    int src_x = 0, src_y = 0;
    Window child;
    XTranslateCoordinates(d, src_win, root, 0, 0, &src_x, &src_y, &child);

    XWindowAttributes win_attrs;
    int src_w = 22, src_h = 22;
    if (XGetWindowAttributes(d, src_win, &win_attrs)) {
        src_w = win_attrs.width;
        src_h = win_attrs.height;
    }

    int screen_w = DisplayWidth(d, tray->screen);
    int screen_h = DisplayHeight(d, tray->screen);

    int pos_x = src_x + (src_w - w) / 2;
    if (pos_x < 4) pos_x = 4;
    if (pos_x + w > screen_w - 4) pos_x = screen_w - w - 4;

    int pos_y;
    if (src_y > screen_h / 2) {
        pos_y = src_y - h - 6;
    } else {
        pos_y = src_y + src_h + 6;
    }

    XMoveResizeWindow(d, tray->tooltip_win, pos_x, pos_y, w, h);
    XMapRaised(d, tray->tooltip_win);
    tray->tooltip_visible = true;
    tray->tooltip_key = (int)key;

    draw_tooltip(tray, text, w, h);
}

static void tray_hide_tooltip(TrayContext *tray) {
    if (!tray || !tray->display || !tray->tooltip_win) return;
    if (tray->tooltip_visible) {
        XUnmapWindow(tray->display, tray->tooltip_win);
        tray->tooltip_visible = false;
        tray->tooltip_key = -999;
        XFlush(tray->display);
    }
}

static void draw_app_icon(TrayContext *tray) {
    Window win = tray->app_icon.window;
    if (!win || !tray->display) return;

    Display *d = tray->display;
    int w = tray->app_icon.width > 0 ? tray->app_icon.width : tray->width;
    int h = tray->app_icon.height > 0 ? tray->app_icon.height : tray->height;

    Pixmap pix = XCreatePixmap(d, win, w, h, DefaultDepth(d, tray->screen));

    if (tray->cairo_available) {
        Visual *visual = DefaultVisual(d, tray->screen);
        cairo_surface_t *surf = tray->cairo.xlib_surface_create(d, pix, visual, w, h);
        if (surf) {
            cairo_t *cr = tray->cairo.create(surf);
            if (cr) {
                double scale = (w < h ? w : h) / 22.0;
                tray->cairo.scale(cr, scale, scale);

                // Background matching panel (#1C1C1C)
                tray->cairo.set_source_rgb(cr, 0x1C / 255.0, 0x1C / 255.0, 0x1C / 255.0);
                tray->cairo.paint(cr);

                // Rounded Keycap surface: 18x18 centered at (2, 2)
                double x = 2.0, y = 2.0, kw = 18.0, kh = 18.0, r = 3.5;
                tray->cairo.new_sub_path(cr);
                tray->cairo.arc(cr, x + kw - r, y + r, r, -1.570796, 0);
                tray->cairo.arc(cr, x + kw - r, y + kh - r, r, 0, 1.570796);
                tray->cairo.arc(cr, x + r, y + kh - r, r, 1.570796, 3.14159);
                tray->cairo.arc(cr, x + r, y + r, r, 3.14159, 4.71239);
                tray->cairo.close_path(cr);

                tray->cairo.set_source_rgb(cr, 0x2E / 255.0, 0x31 / 255.0, 0x36 / 255.0);
                tray->cairo.fill_preserve(cr);

                // Subtle border
                tray->cairo.set_source_rgb(cr, 0x45 / 255.0, 0x48 / 255.0, 0x4F / 255.0);
                tray->cairo.set_line_width(cr, 1.0);
                tray->cairo.stroke(cr);

                // White Arrow Glyph (#F0F2F5)
                tray->cairo.set_source_rgb(cr, 0xF0 / 255.0, 0xF2 / 255.0, 0xF5 / 255.0);
                tray->cairo.move_to(cr, 10.0, 5.0);
                tray->cairo.line_to(cr, 5.5, 9.5);
                tray->cairo.line_to(cr, 8.0, 9.5);
                tray->cairo.line_to(cr, 8.0, 13.0);
                tray->cairo.line_to(cr, 12.0, 13.0);
                tray->cairo.line_to(cr, 12.0, 9.5);
                tray->cairo.line_to(cr, 14.5, 9.5);
                tray->cairo.close_path(cr);
                tray->cairo.fill(cr);

                // Base bar
                double bx = 6.0, by = 14.5, bw = 8.0, bh = 1.6, br = 0.8;
                tray->cairo.new_sub_path(cr);
                tray->cairo.arc(cr, bx + bw - br, by + br, br, -1.570796, 0);
                tray->cairo.arc(cr, bx + bw - br, by + bh - br, br, 0, 1.570796);
                tray->cairo.arc(cr, bx + br, by + bh - br, br, 1.570796, 3.14159);
                tray->cairo.arc(cr, bx + br, by + br, br, 3.14159, 4.71239);
                tray->cairo.close_path(cr);
                tray->cairo.fill(cr);

                // Emerald Green Status LED
                tray->cairo.set_source_rgb(cr, 0x2E / 255.0, 0xA4 / 255.0, 0x4F / 255.0);
                tray->cairo.arc(cr, 16.2, 5.2, 1.4, 0, 6.28318);
                tray->cairo.fill(cr);

                tray->cairo.destroy(cr);
            }
            tray->cairo.surface_destroy(surf);
        }
    } else {
        GC gc = tray->gc;
        XSetForeground(d, gc, 0x1C1C1C);
        XFillRectangle(d, pix, gc, 0, 0, w, h);
        XSetForeground(d, gc, 0x2E3136);
        XFillRectangle(d, pix, gc, 2, 2, w - 4, h - 4);
        XSetForeground(d, gc, 0x2EA44F);
        XDrawRectangle(d, pix, gc, 2, 2, w - 5, h - 5);
        if (tray->font) {
            XSetForeground(d, gc, 0xFFFFFF);
            XDrawString(d, pix, gc, 7, 15, "A", 1);
        }
    }

    if (tray->gc) {
        XCopyArea(d, pix, win, tray->gc, 0, 0, w, h, 0, 0);
    }
    XFreePixmap(d, pix);

    XStoreName(d, win, "CapsLock Status");
    XFlush(d);
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
    XStoreName(d, win, get_key_name(key));

    XFlush(d);
}

static bool create_indicator_window(TrayContext *tray, LockKeyType k) {
    if (!tray || !tray->display || tray->tray_mgr == None) return false;
    if (tray->icons[k].window != None) return true;

    Display *d = tray->display;
    Window root = RootWindow(d, tray->screen);
    Atom xembed_atom = XInternAtom(d, "_XEMBED_INFO", False);
    long xembed_data[2] = {0, 1}; // version 0, XEMBED_MAPPED

    tray->icons[k].key = k;
    tray->icons[k].docked = false;
    tray->icons[k].parent = root;
    tray->icons[k].width = tray->width;
    tray->icons[k].height = tray->height;

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1C1C1C;
    attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | EnterWindowMask | LeaveWindowMask;

    Window win = XCreateWindow(d, root,
                               -200, -200, tray->width, tray->height,
                               0, CopyFromParent, InputOutput,
                               CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWEventMask,
                               &attrs);
    if (!win) return false;

    XChangeProperty(d, win, xembed_atom, xembed_atom, 32,
                    PropModeReplace, (unsigned char *)xembed_data, 2);

    char title[64];
    snprintf(title, sizeof(title), "%s", get_key_name(k));
    XStoreName(d, win, title);

    dock_window(d, tray->tray_mgr, win);
    tray->icons[k].window = win;
    return true;
}

static void destroy_indicator_window(TrayContext *tray, LockKeyType k) {
    if (!tray || !tray->display) return;
    if (tray->tooltip_key == (int)k) {
        tray_hide_tooltip(tray);
    }
    if (tray->icons[k].window != None) {
        XDestroyWindow(tray->display, tray->icons[k].window);
        tray->icons[k].window = None;
    }
    tray->icons[k].docked = false;
}

bool tray_init(TrayContext *tray, Display *d, int screen, const AppConfig *config) {
    if (!tray || !d) return false;
    memset(tray, 0, sizeof(*tray));

    tray->display = d;
    tray->screen = screen;
    tray->width = 22;
    tray->height = 22;
    tray->tooltip_key = -999;

    tray->cairo_available = cairo_api_load(&tray->cairo);

    char tray_atom_name[32];
    snprintf(tray_atom_name, sizeof(tray_atom_name), "_NET_SYSTEM_TRAY_S%d", screen);
    Atom tray_atom = XInternAtom(d, tray_atom_name, True);
    if (tray_atom == None) return false;

    tray->tray_mgr = XGetSelectionOwner(d, tray_atom);
    if (tray->tray_mgr == None) return false;

    Window root = RootWindow(d, screen);
    Atom xembed_atom = XInternAtom(d, "_XEMBED_INFO", False);
    long xembed_data[2] = {0, 1}; // version 0, XEMBED_MAPPED

    // 1. Try to initialize XAppStatusIcon support
    tray->has_xapp = xapp_api_load(&tray->xapp);
    if (tray->has_xapp) {
        char cache_dir[384];
        ensure_icon_cache_dir(cache_dir, sizeof(cache_dir));

        const char *key_slugs[KEY_COUNT] = {"num", "caps", "scroll"};
        for (int k = 0; k < KEY_COUNT; k++) {
            snprintf(tray->icon_paths[k][0], sizeof(tray->icon_paths[k][0]), "%s/%s_off.png", cache_dir, key_slugs[k]);
            snprintf(tray->icon_paths[k][1], sizeof(tray->icon_paths[k][1]), "%s/%s_on.png", cache_dir, key_slugs[k]);

            generate_indicator_png(tray, config, (LockKeyType)k, false, tray->icon_paths[k][0]);
            generate_indicator_png(tray, config, (LockKeyType)k, true,  tray->icon_paths[k][1]);
        }

        // 1a. Create main app icon
        tray->xapp_icon = tray->xapp.icon_new_with_name("capslock-status");
        if (tray->xapp_icon) {
            tray->xapp.set_icon_name(tray->xapp_icon, "capslock-status");
            tray->xapp.set_tooltip_text(tray->xapp_icon, "CapsLock Status");
            tray->xapp.set_visible(tray->xapp_icon, true);
            tray->xapp.g_signal_connect_data(tray->xapp_icon, "button-press-event",
                                             (void *)on_xapp_button_press, tray, NULL, 0);
        }

        // 1b. Initialize indicator icons as NULL; tray_update will create enabled ones
        for (int k = 0; k < KEY_COUNT; k++) {
            tray->xapp_icons[k] = NULL;
        }
    } else {
        // 2. Fallback: Create and dock X11 Main App Icon if XApp is unavailable
        tray->app_icon.key = (LockKeyType)-1;
        tray->app_icon.docked = false;
        tray->app_icon.parent = root;
        tray->app_icon.window = None;
        tray->app_icon.width = tray->width;
        tray->app_icon.height = tray->height;

        XSetWindowAttributes app_attrs;
        memset(&app_attrs, 0, sizeof(app_attrs));
        app_attrs.override_redirect = True;
        app_attrs.background_pixel = 0x1C1C1C;
        app_attrs.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | EnterWindowMask | LeaveWindowMask;

        Window app_win = XCreateWindow(d, root,
                                       -200, -200, tray->width, tray->height,
                                       0, CopyFromParent, InputOutput,
                                       CopyFromParent,
                                       CWOverrideRedirect | CWBackPixel | CWEventMask,
                                       &app_attrs);
        if (app_win) {
            XChangeProperty(d, app_win, xembed_atom, xembed_atom, 32,
                            PropModeReplace, (unsigned char *)xembed_data, 2);
            XStoreName(d, app_win, "CapsLock Status");
            dock_window(d, tray->tray_mgr, app_win);
            tray->app_icon.window = app_win;
        } else {
            return false;
        }

        // 3. Create Fallback Tooltip Window
        XSetWindowAttributes tt_attrs;
        memset(&tt_attrs, 0, sizeof(tt_attrs));
        tt_attrs.override_redirect = True;
        tt_attrs.background_pixel = 0x24272B;
        tt_attrs.event_mask = ExposureMask;

        tray->tooltip_win = XCreateWindow(d, root,
                                          -500, -500, 10, 10,
                                          0, CopyFromParent, InputOutput,
                                          CopyFromParent,
                                          CWOverrideRedirect | CWBackPixel | CWEventMask,
                                          &tt_attrs);
        if (tray->tooltip_win) {
            Atom type_atom = XInternAtom(d, "_NET_WM_WINDOW_TYPE", False);
            Atom tooltip_atom = XInternAtom(d, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
            XChangeProperty(d, tray->tooltip_win, type_atom, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)&tooltip_atom, 1);
        }

        // 4. Setup GC and font
        tray->gc = XCreateGC(d, root, 0, NULL);
        tray->font = XLoadQueryFont(d, "-*-*-bold-r-*-*-11-*-*-*-*-*-*-*");
        if (!tray->font) {
            tray->font = XLoadQueryFont(d, "fixed");
        }
        if (tray->font) {
            XSetFont(d, tray->gc, tray->font->fid);
        }

        // 5. Create lock key indicator icons that are enabled in config
        for (int i = 0; i < KEY_COUNT; i++) {
            tray->icons[i].key = (LockKeyType)i;
            tray->icons[i].docked = false;
            tray->icons[i].parent = root;
            tray->icons[i].window = None;
            tray->icons[i].width = tray->width;
            tray->icons[i].height = tray->height;

            if (is_icon_enabled(config, (LockKeyType)i)) {
                create_indicator_window(tray, (LockKeyType)i);
            }
        }
    }

    return true;
}

void tray_destroy(TrayContext *tray) {
    if (!tray || !tray->display) return;

    tray_hide_tooltip(tray);
    if (tray->tooltip_win != None) {
        XDestroyWindow(tray->display, tray->tooltip_win);
        tray->tooltip_win = None;
    }

    if (tray->has_xapp) {
        if (tray->xapp_icon) {
            tray->xapp.set_visible(tray->xapp_icon, false);
            if (tray->xapp.g_object_unref) {
                tray->xapp.g_object_unref(tray->xapp_icon);
            }
            tray->xapp_icon = NULL;
        }
        for (int k = 0; k < KEY_COUNT; k++) {
            if (tray->xapp_icons[k]) {
                tray->xapp.set_visible(tray->xapp_icons[k], false);
                if (tray->xapp.g_object_unref) {
                    tray->xapp.g_object_unref(tray->xapp_icons[k]);
                }
                tray->xapp_icons[k] = NULL;
            }
        }
    }

    if (tray->app_icon.window) {
        XDestroyWindow(tray->display, tray->app_icon.window);
        tray->app_icon.window = None;
    }
    tray->app_icon.docked = false;

    for (int i = 0; i < KEY_COUNT; i++) {
        destroy_indicator_window(tray, (LockKeyType)i);
    }

    if (tray->font) {
        XFreeFont(tray->display, tray->font);
        tray->font = NULL;
    }
    if (tray->gc) {
        XFreeGC(tray->display, tray->gc);
        tray->gc = NULL;
    }
}

void tray_update(TrayContext *tray, const AppConfig *config, const AppState *state) {
    if (!tray || !tray->display) return;

    if (tray->has_xapp) {
        // Regenerate indicator PNGs in case colors in config changed
        for (int k = 0; k < KEY_COUNT; k++) {
            generate_indicator_png(tray, config, (LockKeyType)k, false, tray->icon_paths[k][0]);
            generate_indicator_png(tray, config, (LockKeyType)k, true,  tray->icon_paths[k][1]);
        }

        const char *icon_dbus_names[KEY_COUNT] = {"capslock-status-num", "capslock-status-caps", "capslock-status-scroll"};

        for (int k = 0; k < KEY_COUNT; k++) {
            bool enabled = is_icon_enabled(config, (LockKeyType)k);
            bool is_on = state ? state->state[k] : false;
            bool want_visible = enabled;
            if (config->tray_only_when_active && !is_on) {
                want_visible = false;
            }

            if (want_visible) {
                if (!tray->xapp_icons[k]) {
                    tray->xapp_icons[k] = tray->xapp.icon_new_with_name(icon_dbus_names[k]);
                    if (tray->xapp_icons[k]) {
                        tray->xapp.set_icon_name(tray->xapp_icons[k], tray->icon_paths[k][is_on ? 1 : 0]);
                        tray->xapp.set_tooltip_text(tray->xapp_icons[k], get_key_name((LockKeyType)k));
                        tray->xapp.set_visible(tray->xapp_icons[k], true);
                        tray->xapp.g_signal_connect_data(tray->xapp_icons[k], "button-press-event",
                                                         (void *)on_xapp_button_press, tray, NULL, 0);
                    }
                } else {
                    tray->xapp.set_icon_name(tray->xapp_icons[k], tray->icon_paths[k][is_on ? 1 : 0]);
                    tray->xapp.set_visible(tray->xapp_icons[k], true);
                }
            } else {
                if (tray->xapp_icons[k]) {
                    tray->xapp.set_visible(tray->xapp_icons[k], false);
                    if (tray->xapp.g_object_unref) {
                        tray->xapp.g_object_unref(tray->xapp_icons[k]);
                    }
                    tray->xapp_icons[k] = NULL;
                }
            }
        }
        tray_iterate(tray);
    } else {
        // 1. App icon: draw X11 fallback icon if docked and not using XApp
        if (tray->app_icon.window != None && tray->app_icon.docked) {
            draw_app_icon(tray);
        }

        // 2. Indicator icons: dynamically created/destroyed based on config and smart tray
        for (int k = 0; k < KEY_COUNT; k++) {
            bool enabled = is_icon_enabled(config, (LockKeyType)k);
            bool is_on = state ? state->state[k] : false;
            bool want_visible = enabled;
            if (config->tray_only_when_active && !is_on) {
                want_visible = false;
            }

            if (want_visible) {
                if (tray->icons[k].window == None) {
                    create_indicator_window(tray, (LockKeyType)k);
                } else if (tray->icons[k].docked) {
                    draw_single_icon(tray, config, (LockKeyType)k, is_on);
                }
            } else {
                if (tray->icons[k].window != None) {
                    destroy_indicator_window(tray, (LockKeyType)k);
                }
            }
        }
    }
}

void tray_handle_event(TrayContext *tray, const AppConfig *config, AppState *state, XEvent *ev,
                       bool *out_open_gui, bool *out_show_menu, int *out_menu_x, int *out_menu_y) {
    if (!tray || !ev) return;

    Window root = RootWindow(tray->display, tray->screen);

    // Handle Tooltip Window expose
    if (tray->tooltip_win != None && ev->xany.window == tray->tooltip_win) {
        if (ev->type == Expose && ev->xexpose.count == 0 && tray->tooltip_visible) {
            const char *text = (tray->tooltip_key == -1) ? "CapsLock Status" : get_key_name((LockKeyType)tray->tooltip_key);
            int w = 0, h = 0;
            get_tooltip_size(tray, text, &w, &h);
            draw_tooltip(tray, text, w, h);
        }
        return;
    }

    // Handle Fallback App Icon events
    if (tray->app_icon.window != None && ev->xany.window == tray->app_icon.window) {
        if (ev->type == Expose && ev->xexpose.count == 0) {
            draw_app_icon(tray);
        } else if (ev->type == ReparentNotify) {
            if (ev->xreparent.parent != root) {
                tray->app_icon.docked = true;
                tray->app_icon.parent = ev->xreparent.parent;
                draw_app_icon(tray);
            } else {
                tray->app_icon.docked = false;
                tray->app_icon.parent = root;
            }
        } else if (ev->type == ConfigureNotify) {
            tray->app_icon.width = ev->xconfigure.width;
            tray->app_icon.height = ev->xconfigure.height;
            if (tray->app_icon.docked) {
                draw_app_icon(tray);
            }
        } else if (ev->type == EnterNotify) {
            tray_show_tooltip(tray, (LockKeyType)-1, tray->app_icon.window);
        } else if (ev->type == LeaveNotify) {
            if (ev->xcrossing.detail != NotifyInferior && tray->tooltip_key == -1) {
                tray_hide_tooltip(tray);
            }
        } else if (ev->type == ButtonPress) {
            tray_hide_tooltip(tray);
            if (ev->xbutton.button == 1) { // Left click: Open Settings GUI
                if (out_open_gui) *out_open_gui = true;
            } else if (ev->xbutton.button == 3) { // Right click: Open Context Menu
                if (out_show_menu) {
                    *out_show_menu = true;
                    if (out_menu_x) *out_menu_x = ev->xbutton.x_root;
                    if (out_menu_y) *out_menu_y = ev->xbutton.y_root;
                }
            }
        }
        return;
    }

    // Handle Indicator Icons events
    for (int k = 0; k < KEY_COUNT; k++) {
        if (tray->icons[k].window != None && ev->xany.window == tray->icons[k].window) {
            if (ev->type == Expose && ev->xexpose.count == 0) {
                if (tray->icons[k].docked && is_icon_enabled(config, (LockKeyType)k)) {
                    draw_single_icon(tray, config, (LockKeyType)k, state->state[k]);
                }
            } else if (ev->type == ReparentNotify) {
                if (ev->xreparent.parent != root) {
                    tray->icons[k].docked = true;
                    tray->icons[k].parent = ev->xreparent.parent;
                    if (is_icon_enabled(config, (LockKeyType)k)) {
                        draw_single_icon(tray, config, (LockKeyType)k, state->state[k]);
                    }
                } else {
                    tray->icons[k].docked = false;
                    tray->icons[k].parent = root;
                }
            } else if (ev->type == ConfigureNotify) {
                tray->icons[k].width = ev->xconfigure.width;
                tray->icons[k].height = ev->xconfigure.height;
                if (tray->icons[k].docked && is_icon_enabled(config, (LockKeyType)k)) {
                    draw_single_icon(tray, config, (LockKeyType)k, state->state[k]);
                }
            } else if (ev->type == EnterNotify) {
                tray_show_tooltip(tray, (LockKeyType)k, tray->icons[k].window);
            } else if (ev->type == LeaveNotify) {
                if (ev->xcrossing.detail != NotifyInferior && tray->tooltip_key == (int)k) {
                    tray_hide_tooltip(tray);
                }
            } else if (ev->type == ButtonPress) {
                tray_hide_tooltip(tray);
                if (ev->xbutton.button == 1) { // Left click: Open Settings GUI
                    if (out_open_gui) *out_open_gui = true;
                } else if (ev->xbutton.button == 3) { // Right click: Open Context Menu
                    if (out_show_menu) {
                        *out_show_menu = true;
                        if (out_menu_x) *out_menu_x = ev->xbutton.x_root;
                        if (out_menu_y) *out_menu_y = ev->xbutton.y_root;
                    }
                }
            }
            break;
        }
    }
}
