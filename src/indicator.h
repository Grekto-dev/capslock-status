#ifndef INDICATOR_H
#define INDICATOR_H

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <stdbool.h>

#define APP_NAME "CapsLock Status"
#define APP_VERSION "1.0.0"
#define DEFAULT_CONFIG_FILE "config.ini"
#define CONFIG_DIR_NAME "capslock-status"
#define LEGACY_CONFIG_DIR_NAME "capslock-indicator"

typedef enum {
    POS_TOP_LEFT,
    POS_TOP_CENTER,
    POS_TOP_RIGHT,
    POS_MIDDLE_LEFT,
    POS_CENTER,
    POS_MIDDLE_RIGHT,
    POS_BOTTOM_LEFT,
    POS_BOTTOM_CENTER,
    POS_BOTTOM_RIGHT
} OSDPosition;

typedef enum {
    KEY_NUM_LOCK = 0,
    KEY_CAPS_LOCK = 1,
    KEY_SCROLL_LOCK = 2,
    KEY_COUNT = 3
} LockKeyType;

typedef struct {
    // Show icons for... (tray icons)
    bool icon_num;
    bool icon_caps;
    bool icon_scroll;

    // Show notification when... changed (OSD overlay)
    bool notify_num;
    bool notify_caps;
    bool notify_scroll;

    // Checkboxes below groups
    bool show_no_icons;
    bool show_no_notification;
    bool start_on_logon;
    bool hide_on_startup;

    // Smart tray: only show icons when active
    bool tray_only_when_active;

    // Persistent overlays
    bool persistent_caps_on;
    bool persistent_caps_off;
    bool persistent_num_on;
    bool persistent_num_off;
    bool persistent_scroll_on;
    bool persistent_scroll_off;

    // OSD appearance (matches Windows cli_preview.png: 176x42, #424242 bg, #4DB434/#B4344D border)
    bool show_osd;
    OSDPosition position;
    int margin_x;
    int margin_y;
    int duration_ms;           // Milliseconds to display OSD (default 1200)
    int border_width;          // default 2px
    bool sticky;               // Keep visible while key is active
    bool dismiss_on_click;     // Close immediately when clicked

    // Exact Windows original colors
    unsigned long bg_color;    // 0x424242 (RGB 66, 66, 66)
    unsigned long border_on;   // 0x4DB434 (RGB 77, 180, 52 - Green)
    unsigned long border_off;  // 0xB4344D (RGB 180, 52, 77 - Red)
    unsigned long text_color;  // 0xFFFFFF (White)

    // Language and text messages
    char lang[16];             // "en-GB" or "pt-BR"
    char text_caps_on[64];     // "Caps lock is on"
    char text_caps_off[64];    // "Caps lock is off"
    char text_num_on[64];      // "Num lock is on"
    char text_num_off[64];     // "Num lock is off"
    char text_scroll_on[64];   // "Scroll lock is on"
    char text_scroll_off[64];  // "Scroll lock is off"

    // Sound alert
    bool enable_sound;
    char sound_on_cmd[256];
    char sound_off_cmd[256];
} AppConfig;

typedef struct {
    bool state[KEY_COUNT];     // Current state of each lock key (true = ON)
    int xkb_indices[KEY_COUNT];// XKB indicator index for each key
    unsigned int caps_mask;    // Modifier mask for Caps Lock (LockMask)
    unsigned int num_mask;     // Modifier mask for Num Lock (Mod2Mask)
    unsigned int scroll_mask;  // Modifier mask for Scroll Lock (Mod3Mask)
    int xkb_event_code;        // Base event code for XKB events
    Display *display;
    int screen;
    Window root;
} AppState;

// Functions
bool indicator_init(AppState *app, Display *display);
void indicator_query_state(AppState *app);
bool indicator_is_on(const AppState *app, LockKeyType key);
bool indicator_toggle(AppState *app, LockKeyType key);
const char *indicator_get_status_text(const AppConfig *config, LockKeyType key, bool is_on);
bool indicator_is_persistent(const AppConfig *config, LockKeyType key, bool is_on);

#endif // INDICATOR_H
