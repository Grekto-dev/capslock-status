#define _GNU_SOURCE
#include "osd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

// Sets _NET_WM_WINDOW_OPACITY without flushing, so callers can batch several requests into one write.
static void osd_set_opacity_nf(OSDContext *osd, float opacity) {
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    osd->current_opacity = opacity;

    // Compute in double and clamp: in float, 1.0f * 0xFFFFFFFF rounds up to 2^32, which overflows
    // uint32_t and wraps to 0 (fully transparent) - a one-frame blink at the start of the fade-out.
    double scaled = (double)opacity * 4294967295.0;
    if (scaled < 0.0) scaled = 0.0;
    if (scaled > 4294967295.0) scaled = 4294967295.0;
    uint32_t val = (uint32_t)scaled;
    XChangeProperty(osd->display, osd->window, osd->atom_net_opacity, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&val, 1);
}

// Parking spot used while the (always-mapped) OSD window is invisible.
static int osd_hidden_x(const OSDContext *osd) { return -(osd->width + 200); }
static int osd_hidden_y(const OSDContext *osd) { return -(osd->height + 200); }


static void calculate_coords(Display *d, int screen, int win_w, int win_h, const AppConfig *config, int *out_x, int *out_y) {
    int sw = DisplayWidth(d, screen);
    int sh = DisplayHeight(d, screen);

    int x = 0, y = 0;
    int mx = config->margin_x;
    int my = config->margin_y;

    switch (config->position) {
        case POS_TOP_LEFT:
            x = mx;
            y = my;
            break;
        case POS_TOP_CENTER:
            x = (sw - win_w) / 2;
            y = my;
            break;
        case POS_TOP_RIGHT:
            x = sw - win_w - mx;
            y = my;
            break;
        case POS_MIDDLE_LEFT:
            x = mx;
            y = (sh - win_h) / 2;
            break;
        case POS_CENTER:
            x = (sw - win_w) / 2;
            y = (sh - win_h) / 2;
            break;
        case POS_MIDDLE_RIGHT:
            x = sw - win_w - mx;
            y = (sh - win_h) / 2;
            break;
        case POS_BOTTOM_LEFT:
            x = mx;
            y = sh - win_h - my;
            break;
        case POS_BOTTOM_CENTER:
            x = (sw - win_w) / 2;
            y = sh - win_h - my;
            break;
        case POS_BOTTOM_RIGHT:
        default:
            x = sw - win_w - mx;
            y = sh - win_h - my;
            break;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    *out_x = x;
    *out_y = y;
}

bool osd_init(OSDContext *osd, Display *d, int screen, const AppConfig *config) {
    if (!osd || !d) return false;
    memset(osd, 0, sizeof(*osd));

    osd->display = d;
    osd->screen = screen;
    osd->width = 176;
    osd->height = 42;
    osd->visible = false;
    osd->current_key = KEY_CAPS_LOCK;
    osd->current_state = true;
    osd->current_opacity = 1.0f;
    osd->atom_net_opacity = XInternAtom(d, "_NET_WM_WINDOW_OPACITY", False);

    int win_x = 0, win_y = 0;
    calculate_coords(d, screen, osd->width, osd->height, config, &win_x, &win_y);

    Window root = RootWindow(d, screen);

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = config->bg_color;
    attrs.border_pixel = config->border_on;

    osd->window = XCreateWindow(d, root, win_x, win_y, osd->width, osd->height,
                                config->border_width,
                                CopyFromParent, InputOutput,
                                CopyFromParent,
                                CWOverrideRedirect | CWBackPixel | CWBorderPixel,
                                &attrs);

    if (!osd->window) {
        fprintf(stderr, "Error: Failed to create OSD window.\n");
        return false;
    }

    // Set Window type to Notification
    Atom net_wm_win_type = XInternAtom(d, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_type_notification = XInternAtom(d, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(d, osd->window, net_wm_win_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&net_wm_type_notification, 1);

    // Keep window on top
    Atom net_wm_state = XInternAtom(d, "_NET_WM_STATE", False);
    Atom net_wm_state_above = XInternAtom(d, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(d, osd->window, net_wm_state, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&net_wm_state_above, 1);

    XSelectInput(d, osd->window, ExposureMask | ButtonPressMask);

    osd->pixmap = XCreatePixmap(d, osd->window, osd->width, osd->height, DefaultDepth(d, screen));
    osd->gc = XCreateGC(d, osd->window, 0, NULL);

    // Initialize Cairo dynamically
    osd->cairo_available = cairo_api_load(&osd->cairo);

    char **missing = NULL;
    int missing_count = 0;
    char *def_str = NULL;
    osd->font_title = XCreateFontSet(d,
        "-*-segoe ui-medium-r-*-*-14-*-*-*-*-*-*-*,-*-dejavu sans-medium-r-*-*-14-*-*-*-*-*-*-*,-*-liberation sans-medium-r-*-*-14-*-*-*-*-*-*-*,-*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-*,-*-*-medium-r-*-*-14-*-*-*-*-*-*-*,fixed",
        &missing, &missing_count, &def_str);

    // Pre-map the window once, fully transparent and off-screen. Showing the OSD later is then
    // just "draw + move + opacity" in one batched request: no map/unmap round trip and no
    // compositor map animation on each key press (this is what makes it feel instant).
    osd_set_opacity_nf(osd, 0.0f);
    XMoveWindow(d, osd->window, osd_hidden_x(osd), osd_hidden_y(osd));
    XMapWindow(d, osd->window);
    XFlush(d);

    return true;
}

void osd_destroy(OSDContext *osd) {
    if (!osd || !osd->display) return;

    // Do NOT dlclose Cairo handle: Cairo registers display-close hooks in X11
    // that are invoked during XCloseDisplay(). Unmapping Cairo causes a segfault.
    osd->cairo_available = false;

    if (osd->pixmap) {
        XFreePixmap(osd->display, osd->pixmap);
        osd->pixmap = None;
    }
    if (osd->gc) {
        XFreeGC(osd->display, osd->gc);
        osd->gc = NULL;
    }
    if (osd->font_title) {
        XFreeFontSet(osd->display, osd->font_title);
        osd->font_title = NULL;
    }
    if (osd->window) {
        XDestroyWindow(osd->display, osd->window);
        osd->window = None;
    }
    osd->visible = false;
}

void osd_set_opacity(OSDContext *osd, float opacity) {
    if (!osd || !osd->display || !osd->window) return;
    osd_set_opacity_nf(osd, opacity);
    XFlush(osd->display);
}

// Draws the current state into the pixmap and copies it to the window (no flush).
static void osd_draw(OSDContext *osd, const AppConfig *config) {
    Display *d = osd->display;
    Pixmap pix = osd->pixmap;
    GC gc = osd->gc;
    int w = osd->width;
    int h = osd->height;

    const char *text = indicator_get_status_text(config, osd->current_key, osd->current_state);

    if (osd->cairo_available) {
        Visual *visual = DefaultVisual(d, osd->screen);
        cairo_surface_t *surf = osd->cairo.xlib_surface_create(d, pix, visual, w, h);
        if (surf) {
            cairo_t *cr = osd->cairo.create(surf);
            if (cr) {
                // 1. Solid background #424242 (matching cli_preview.png)
                double bg_r = ((config->bg_color >> 16) & 0xFF) / 255.0;
                double bg_g = ((config->bg_color >> 8) & 0xFF) / 255.0;
                double bg_b = (config->bg_color & 0xFF) / 255.0;
                osd->cairo.set_source_rgb(cr, bg_r, bg_g, bg_b);
                osd->cairo.rectangle(cr, 0, 0, w, h);
                osd->cairo.fill(cr);

                // 2. Solid border: #4DB434 (Green) if active, #B4344D (Red) if inactive
                unsigned long border_col = osd->current_state ? config->border_on : config->border_off;
                double b_r = ((border_col >> 16) & 0xFF) / 255.0;
                double b_g = ((border_col >> 8) & 0xFF) / 255.0;
                double b_b = (border_col & 0xFF) / 255.0;
                osd->cairo.set_source_rgb(cr, b_r, b_g, b_b);
                double bw = config->border_width > 0 ? (double)config->border_width : 2.0;
                osd->cairo.set_line_width(cr, bw);
                double half_bw = bw / 2.0;
                osd->cairo.rectangle(cr, half_bw, half_bw, w - bw, h - bw);
                osd->cairo.stroke(cr);

                // 3. Status text in Segoe UI font with subpixel anti-aliasing
                if (text && text[0] != '\0') {
                    osd->cairo.select_font_face(cr, "Segoe UI", 0, 0);
                    osd->cairo.set_font_size(cr, 15.0);
                    osd->cairo.set_antialias(cr, 3 /* CAIRO_ANTIALIAS_SUBPIXEL */);

                    cairo_text_extents_t ext;
                    osd->cairo.text_extents(cr, text, &ext);

                    double tx = (w - ext.width) / 2.0 - ext.x_bearing;
                    double ty = (h - ext.height) / 2.0 - ext.y_bearing;

                    double t_r = ((config->text_color >> 16) & 0xFF) / 255.0;
                    double t_g = ((config->text_color >> 8) & 0xFF) / 255.0;
                    double t_b = (config->text_color & 0xFF) / 255.0;
                    osd->cairo.set_source_rgb(cr, t_r, t_g, t_b);
                    osd->cairo.move_to(cr, tx, ty);
                    osd->cairo.show_text(cr, text);
                }

                osd->cairo.destroy(cr);
            }
            osd->cairo.surface_destroy(surf);
        }
    } else {
        // Fallback X11 drawing
        XSetForeground(d, gc, config->bg_color);
        XFillRectangle(d, pix, gc, 0, 0, w, h);

        unsigned long border_col = osd->current_state ? config->border_on : config->border_off;
        XSetForeground(d, gc, border_col);
        XSetLineAttributes(d, gc, config->border_width, LineSolid, CapButt, JoinMiter);
        XDrawRectangle(d, pix, gc, 0, 0, w - 1, h - 1);

        if (osd->font_title && text) {
            XRectangle ink, logical;
            Xutf8TextExtents(osd->font_title, text, strlen(text), &ink, &logical);

            int text_x = (w - logical.width) / 2;
            int text_y = (h + logical.height) / 2 - 2;

            XSetForeground(d, gc, config->text_color);
            Xutf8DrawString(d, pix, osd->font_title, gc, text_x, text_y, text, strlen(text));
        }
    }

    // Copy Pixmap to Window
    XCopyArea(d, pix, osd->window, gc, 0, 0, w, h, 0, 0);
}

void osd_redraw(OSDContext *osd, const AppConfig *config) {
    if (!osd || !osd->display || !osd->pixmap) return;
    osd_draw(osd, config);
    XFlush(osd->display);
}

void osd_show(OSDContext *osd, const AppConfig *config, LockKeyType key, bool is_on) {
    if (!osd || !osd->display || !osd->window) return;

    osd->current_key = key;
    osd->current_state = is_on;
    osd->total_duration_ms = config->duration_ms > 0 ? config->duration_ms : 1200;

    // Start the timeline first: the clock must begin at the key press, not after X round trips.
    clock_gettime(CLOCK_MONOTONIC, &osd->show_time);

    // Compute text length to dynamically size the box if text is longer
    const char *text = indicator_get_status_text(config, key, is_on);
    int target_w = 176;
    if (text) {
        if (osd->cairo_available) {
            Visual *visual = DefaultVisual(osd->display, osd->screen);
            cairo_surface_t *surf = osd->cairo.xlib_surface_create(osd->display, osd->pixmap, visual, osd->width, osd->height);
            if (surf) {
                cairo_t *cr = osd->cairo.create(surf);
                if (cr) {
                    osd->cairo.select_font_face(cr, "Segoe UI", 0, 0);
                    osd->cairo.set_font_size(cr, 15.0);
                    cairo_text_extents_t ext;
                    osd->cairo.text_extents(cr, text, &ext);
                    int needed_w = (int)(ext.width + 36.5);
                    if (needed_w > target_w) target_w = needed_w;
                    osd->cairo.destroy(cr);
                }
                osd->cairo.surface_destroy(surf);
            }
        } else if (osd->font_title) {
            XRectangle ink, logical;
            Xutf8TextExtents(osd->font_title, text, strlen(text), &ink, &logical);
            int needed_w = logical.width + 36;
            if (needed_w > target_w) target_w = needed_w;
        }
    }

    // Resize window and pixmap if width changed (only happens for longer custom texts)
    if (target_w != osd->width) {
        osd->width = target_w;
        if (osd->pixmap) {
            XFreePixmap(osd->display, osd->pixmap);
            osd->pixmap = XCreatePixmap(osd->display, osd->window, osd->width, osd->height,
                                        DefaultDepth(osd->display, osd->screen));
        }
        XResizeWindow(osd->display, osd->window, osd->width, osd->height);
    }

    int win_x = 0, win_y = 0;
    calculate_coords(osd->display, osd->screen, osd->width, osd->height, config, &win_x, &win_y);

    // Batch everything into ONE write to the X server, ordered so the compositor never shows a
    // stale frame: 1) paint the new content while still off-screen/transparent,
    // 2) move into place, 3) raise, 4) make opaque last.
    osd_draw(osd, config);
    XMoveWindow(osd->display, osd->window, win_x, win_y);
    XRaiseWindow(osd->display, osd->window);
    osd_set_opacity_nf(osd, 1.0f);
    XFlush(osd->display);
    osd->visible = true;

    // Timeout calculation
    if (indicator_is_persistent(config, key, is_on)) {
        osd->hide_time.tv_sec = 0;
        osd->hide_time.tv_nsec = 0;
    } else {
        osd->hide_time = osd->show_time;
        long sec = osd->total_duration_ms / 1000;
        long nsec = (osd->total_duration_ms % 1000) * 1000000L;
        osd->hide_time.tv_sec += sec;
        osd->hide_time.tv_nsec += nsec;
        if (osd->hide_time.tv_nsec >= 1000000000L) {
            osd->hide_time.tv_sec += 1;
            osd->hide_time.tv_nsec -= 1000000000L;
        }
    }
}

void osd_hide(OSDContext *osd) {
    if (!osd || !osd->display || !osd->window) return;
    if (osd->visible) {
        // Keep the window mapped: just make it transparent and park it off-screen.
        osd_set_opacity_nf(osd, 0.0f);
        XMoveWindow(osd->display, osd->window, osd_hidden_x(osd), osd_hidden_y(osd));
        XFlush(osd->display);
        osd->visible = false;
    }
}

bool osd_is_visible(const OSDContext *osd) {
    return osd ? osd->visible : false;
}

void osd_update_animation(OSDContext *osd, const AppConfig *config) {
    if (!osd || !osd->visible) return;
    if (indicator_is_persistent(config, osd->current_key, osd->current_state)) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long elapsed_ms = (now.tv_sec - osd->show_time.tv_sec) * 1000 +
                      (now.tv_nsec - osd->show_time.tv_nsec) / 1000000L;

    if (elapsed_ms >= osd->total_duration_ms) {
        osd_hide(osd);
        return;
    }

    // Exact Windows CapsLockIndicator timing: 50% solid hold, 50% linear fade-out
    long hold_time = osd->total_duration_ms / 2;

    if (elapsed_ms < hold_time) {
        if (osd->current_opacity != 1.0f) {
            osd_set_opacity(osd, 1.0f);
        }
    } else {
        float progress = (float)(elapsed_ms - hold_time) / (float)(osd->total_duration_ms - hold_time);
        if (progress > 1.0f) progress = 1.0f;
        float opacity = 1.0f - progress;
        osd_set_opacity(osd, opacity);
    }
}

long osd_get_remaining_ms(const OSDContext *osd) {
    if (!osd || !osd->visible) return -1;
    if (osd->hide_time.tv_sec == 0 && osd->hide_time.tv_nsec == 0) {
        return -1; // Persistent
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long elapsed_ms = (now.tv_sec - osd->show_time.tv_sec) * 1000 +
                      (now.tv_nsec - osd->show_time.tv_nsec) / 1000000L;

    if (elapsed_ms >= osd->total_duration_ms) {
        return 0; // expired
    }

    long hold_time = osd->total_duration_ms / 2;
    if (elapsed_ms < hold_time) {
        long until_fade = hold_time - elapsed_ms;
        return (until_fade > 16) ? 16 : (until_fade > 0 ? until_fade : 1);
    } else {
        long remaining = osd->total_duration_ms - elapsed_ms;
        return (remaining > 16) ? 16 : (remaining > 0 ? remaining : 1);
    }
}

void osd_handle_event(OSDContext *osd, const AppConfig *config, XEvent *ev) {
    if (!osd || !ev) return;
    if (ev->xany.window != osd->window) return;

    if (ev->type == Expose && ev->xexpose.count == 0) {
        osd_redraw(osd, config);
    } else if (ev->type == ButtonPress) {
        if (config->dismiss_on_click) {
            osd_hide(osd);
        }
    }
}
