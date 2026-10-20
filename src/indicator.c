#include "indicator.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <X11/keysym.h>

bool indicator_init(AppState *app, Display *display) {
    if (!app || !display) return false;

    app->display = display;
    app->screen = DefaultScreen(display);
    app->root = RootWindow(display, app->screen);

    app->xkb_indices[KEY_NUM_LOCK] = 1;
    app->xkb_indices[KEY_CAPS_LOCK] = 0;
    app->xkb_indices[KEY_SCROLL_LOCK] = 2;

    int xkb_opcode, xkb_event, xkb_error;
    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;

    if (!XkbQueryExtension(display, &xkb_opcode, &xkb_event, &xkb_error, &major, &minor)) {
        fprintf(stderr, "Error: XKB extension is not available on this X server.\n");
        return false;
    }

    app->xkb_event_code = xkb_event;

    // Dynamically resolve modifier masks for instantaneous keypress response (0ms perceived delay)
    app->caps_mask = XkbKeysymToModifiers(display, XK_Caps_Lock);
    if (!app->caps_mask) app->caps_mask = LockMask;

    app->num_mask = XkbKeysymToModifiers(display, XK_Num_Lock);
    if (!app->num_mask) app->num_mask = Mod2Mask;

    app->scroll_mask = XkbKeysymToModifiers(display, XK_Scroll_Lock);
    if (!app->scroll_mask) app->scroll_mask = Mod3Mask;

    XkbDescPtr xkb = XkbGetKeyboard(display, XkbAllComponentsMask, XkbUseCoreKbd);
    if (xkb && xkb->names) {
        for (int i = 0; i < XkbNumIndicators; i++) {
            if (xkb->names->indicators[i] != None) {
                char *name = XGetAtomName(display, xkb->names->indicators[i]);
                if (name) {
                    if (strcasecmp(name, "Caps Lock") == 0) {
                        app->xkb_indices[KEY_CAPS_LOCK] = i;
                    } else if (strcasecmp(name, "Num Lock") == 0) {
                        app->xkb_indices[KEY_NUM_LOCK] = i;
                    } else if (strcasecmp(name, "Scroll Lock") == 0) {
                        app->xkb_indices[KEY_SCROLL_LOCK] = i;
                    }
                    XFree(name);
                }
            }
        }
    }
    if (xkb) {
        XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);
    }

    // Subscribe to both general XKB events and explicit event details
    XkbSelectEvents(display, XkbUseCoreKbd,
                    XkbStateNotifyMask | XkbIndicatorStateNotifyMask,
                    XkbStateNotifyMask | XkbIndicatorStateNotifyMask);

    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify,
                          XkbAllStateComponentsMask, XkbAllStateComponentsMask);

    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbIndicatorStateNotify,
                          XkbAllIndicatorsMask, XkbAllIndicatorsMask);

    indicator_query_state(app);

    return true;
}

void indicator_query_state(AppState *app) {
    if (!app || !app->display) return;

    XkbStateRec xkb_state;
    memset(&xkb_state, 0, sizeof(xkb_state));
    XkbGetState(app->display, XkbUseCoreKbd, &xkb_state);

    unsigned int state_mask = 0;
    XkbGetIndicatorState(app->display, XkbUseCoreKbd, &state_mask);

    bool ind_scroll = (state_mask & (1U << app->xkb_indices[KEY_SCROLL_LOCK])) != 0;

    // Use canonical locked modifiers directly for Caps Lock and Num Lock.
    // Critical: NEVER OR-latch with physical LED indicator state (ind_caps/ind_num),
    // because the hardware/kernel LED subsystem feedback loop can take 500-1000ms to update,
    // which would cause an intolerable ~1s lag when turning off the keys!
    app->state[KEY_CAPS_LOCK]   = (xkb_state.locked_mods & app->caps_mask) != 0;
    app->state[KEY_NUM_LOCK]    = (xkb_state.locked_mods & app->num_mask) != 0;
    app->state[KEY_SCROLL_LOCK] = ((xkb_state.locked_mods & app->scroll_mask) != 0) || ind_scroll;
}

bool indicator_is_on(const AppState *app, LockKeyType key) {
    if (!app || key < 0 || key >= KEY_COUNT) return false;
    return app->state[key];
}

bool indicator_toggle(AppState *app, LockKeyType key) {
    if (!app || !app->display) return false;

    indicator_query_state(app);

    unsigned int affect_mask = 0;
    if (key == KEY_CAPS_LOCK) {
        affect_mask = app->caps_mask;
    } else if (key == KEY_NUM_LOCK) {
        affect_mask = app->num_mask;
    } else if (key == KEY_SCROLL_LOCK) {
        affect_mask = app->scroll_mask;
    } else {
        return false;
    }

    bool current = app->state[key];
    unsigned int values = current ? 0 : affect_mask;

    XkbLockModifiers(app->display, XkbUseCoreKbd, affect_mask, values);
    XFlush(app->display);

    indicator_query_state(app);
    return true;
}

const char *indicator_get_status_text(const AppConfig *config, LockKeyType key, bool is_on) {
    if (!config) return is_on ? "ON" : "OFF";
    switch (key) {
        case KEY_CAPS_LOCK:
            return is_on ? config->text_caps_on : config->text_caps_off;
        case KEY_NUM_LOCK:
            return is_on ? config->text_num_on : config->text_num_off;
        case KEY_SCROLL_LOCK:
            return is_on ? config->text_scroll_on : config->text_scroll_off;
        default:
            return is_on ? "ON" : "OFF";
    }
}

bool indicator_is_persistent(const AppConfig *config, LockKeyType key, bool is_on) {
    if (!config) return false;
    if (config->sticky && is_on) return true;

    switch (key) {
        case KEY_CAPS_LOCK:
            return is_on ? config->persistent_caps_on : config->persistent_caps_off;
        case KEY_NUM_LOCK:
            return is_on ? config->persistent_num_on : config->persistent_num_off;
        case KEY_SCROLL_LOCK:
            return is_on ? config->persistent_scroll_on : config->persistent_scroll_off;
        default:
            return false;
    }
}
