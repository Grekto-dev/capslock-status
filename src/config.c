#include "config.h"
#include "i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_config_path[512] = {0};

const char *config_get_default_path(void) {
    if (g_config_path[0] != '\0') return g_config_path;

    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    if (xdg && xdg[0] != '\0') {
        snprintf(g_config_path, sizeof(g_config_path), "%s/%s/%s", xdg, CONFIG_DIR_NAME, DEFAULT_CONFIG_FILE);
    } else {
        snprintf(g_config_path, sizeof(g_config_path), "%s/.config/%s/%s", home, CONFIG_DIR_NAME, DEFAULT_CONFIG_FILE);
    }

    if (access(g_config_path, F_OK) != 0) {
        static char legacy_path[512];
        snprintf(legacy_path, sizeof(legacy_path), "%s/.config/%s/%s", home, LEGACY_CONFIG_DIR_NAME, DEFAULT_CONFIG_FILE);
        if (access(legacy_path, F_OK) == 0) {
            return legacy_path;
        }
    }
    return g_config_path;
}

static void ensure_dir_exists(const char *filepath) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", filepath);
    char *p = strrchr(dir, '/');
    if (p) {
        *p = '\0';
        char parent[512];
        snprintf(parent, sizeof(parent), "%s", dir);
        char *p2 = strrchr(parent, '/');
        if (p2) {
            *p2 = '\0';
            mkdir(parent, 0755);
        }
        mkdir(dir, 0755);
    }
}

// Language code matching the desktop locale ($LC_ALL / $LC_MESSAGES / $LANG), English if unknown.
static const char *system_language_code(void) {
    const char *loc = getenv("LC_ALL");
    if (!loc || !*loc) loc = getenv("LC_MESSAGES");
    if (!loc || !*loc) loc = getenv("LANG");
    return i18n_get(loc)->code;
}

// Fills the six OSD texts from the selected language. A text is only kept as-is when the user
// customised it (i.e. it is not empty and not one of the built-in texts of any language), so
// switching language immediately translates everything that was never customised.
static void apply_language_texts(AppConfig *config) {
    const Lang *lang = i18n_get(config->lang);
    struct { char *dst; size_t size; int key; bool on; } map[] = {
        { config->text_num_on,     sizeof(config->text_num_on),     KEY_NUM_LOCK,    true  },
        { config->text_num_off,    sizeof(config->text_num_off),    KEY_NUM_LOCK,    false },
        { config->text_caps_on,    sizeof(config->text_caps_on),    KEY_CAPS_LOCK,   true  },
        { config->text_caps_off,   sizeof(config->text_caps_off),   KEY_CAPS_LOCK,   false },
        { config->text_scroll_on,  sizeof(config->text_scroll_on),  KEY_SCROLL_LOCK, true  },
        { config->text_scroll_off, sizeof(config->text_scroll_off), KEY_SCROLL_LOCK, false },
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (i18n_is_builtin_text(map[i].dst)) {
            i18n_status_text(lang, map[i].key, map[i].on, map[i].dst, map[i].size);
        }
    }
}

void config_set_defaults(AppConfig *config) {
    if (!config) return;

    // Show icons for...
    config->icon_num = true;
    config->icon_caps = true;
    config->icon_scroll = true;

    // Show notification when... changed
    config->notify_num = true;
    config->notify_caps = true;
    config->notify_scroll = true;

    // Bottom checkboxes
    config->show_no_icons = false;
    config->show_no_notification = false;
    config->start_on_logon = true;
    config->hide_on_startup = true;

    // OSD appearance (identical to cli_preview.png)
    config->show_osd = true;
    config->position = POS_BOTTOM_RIGHT;
    config->margin_x = 24;
    config->margin_y = 52;
    config->duration_ms = 500;
    config->border_width = 2;
    config->sticky = false;
    config->dismiss_on_click = true;

    // Smart tray & persistent overlays
    config->tray_only_when_active = false;
    config->persistent_caps_on = false;
    config->persistent_caps_off = false;
    config->persistent_num_on = false;
    config->persistent_num_off = false;
    config->persistent_scroll_on = false;
    config->persistent_scroll_off = false;

    // Exact Windows original colors from cli_preview.png
    config->bg_color = 0x424242;       // RGB(66, 66, 66)
    config->border_on = 0x4DB434;      // RGB(77, 180, 52) - Green
    config->border_off = 0xB4344D;     // RGB(180, 52, 77) - Red
    config->text_color = 0xFFFFFF;     // Pure White

    // Language (follows the desktop locale until the user picks one) and OSD texts
    memset(config->lang, 0, sizeof(config->lang));
    strncpy(config->lang, system_language_code(), sizeof(config->lang) - 1);
    memset(config->text_num_on, 0, sizeof(config->text_num_on));
    memset(config->text_num_off, 0, sizeof(config->text_num_off));
    memset(config->text_caps_on, 0, sizeof(config->text_caps_on));
    memset(config->text_caps_off, 0, sizeof(config->text_caps_off));
    memset(config->text_scroll_on, 0, sizeof(config->text_scroll_on));
    memset(config->text_scroll_off, 0, sizeof(config->text_scroll_off));
    apply_language_texts(config);

    // Sound
    config->enable_sound = false;
    config->sound_on_cmd[0] = '\0';
    config->sound_off_cmd[0] = '\0';
}

static char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static bool parse_bool(const char *val) {
    if (!val) return false;
    return (strcasecmp(val, "true") == 0 ||
            strcasecmp(val, "yes") == 0 ||
            strcasecmp(val, "1") == 0 ||
            strcasecmp(val, "on") == 0);
}

static unsigned long parse_color(const char *val, unsigned long def) {
    if (!val || val[0] == '\0') return def;
    if (val[0] == '#') val++;
    else if (val[0] == '0' && (val[1] == 'x' || val[1] == 'X')) val += 2;
    char *end = NULL;
    unsigned long res = strtoul(val, &end, 16);
    if (end == val) return def;
    return res;
}

OSDPosition config_parse_position(const char *str) {
    if (!str) return POS_BOTTOM_RIGHT;
    if (strcasecmp(str, "top-left") == 0) return POS_TOP_LEFT;
    if (strcasecmp(str, "top-center") == 0) return POS_TOP_CENTER;
    if (strcasecmp(str, "top-right") == 0) return POS_TOP_RIGHT;
    if (strcasecmp(str, "middle-left") == 0) return POS_MIDDLE_LEFT;
    if (strcasecmp(str, "center") == 0) return POS_CENTER;
    if (strcasecmp(str, "middle-right") == 0) return POS_MIDDLE_RIGHT;
    if (strcasecmp(str, "bottom-left") == 0) return POS_BOTTOM_LEFT;
    if (strcasecmp(str, "bottom-center") == 0) return POS_BOTTOM_CENTER;
    if (strcasecmp(str, "bottom-right") == 0) return POS_BOTTOM_RIGHT;
    return POS_BOTTOM_RIGHT;
}

const char *config_position_to_string(OSDPosition pos) {
    switch (pos) {
        case POS_TOP_LEFT: return "top-left";
        case POS_TOP_CENTER: return "top-center";
        case POS_TOP_RIGHT: return "top-right";
        case POS_MIDDLE_LEFT: return "middle-left";
        case POS_CENTER: return "center";
        case POS_MIDDLE_RIGHT: return "middle-right";
        case POS_BOTTOM_LEFT: return "bottom-left";
        case POS_BOTTOM_CENTER: return "bottom-center";
        case POS_BOTTOM_RIGHT: return "bottom-right";
        default: return "bottom-right";
    }
}

bool config_save_default_if_missing(const char *path) {
    if (!path) path = config_get_default_path();
    if (access(path, F_OK) == 0) {
        return true;
    }

    ensure_dir_exists(path);

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "# ====================================================================\n");
    fprintf(f, "# CapsLock Indicator - Configuration File (Linux Port)\n");
    fprintf(f, "# Exact 1:1 replica of Jonas Kohl's CapsLock Indicator\n");
    fprintf(f, "# ====================================================================\n\n");

    fprintf(f, "[icons]\n");
    fprintf(f, "# Show icons for...\n");
    fprintf(f, "num_lock = true\n");
    fprintf(f, "caps_lock = true\n");
    fprintf(f, "scroll_lock = true\n");
    fprintf(f, "show_no_icons = false\n\n");

    fprintf(f, "[notifications]\n");
    fprintf(f, "# Show notification when... changed\n");
    fprintf(f, "num_lock = true\n");
    fprintf(f, "caps_lock = true\n");
    fprintf(f, "scroll_lock = true\n");
    fprintf(f, "show_no_notification = false\n\n");

    fprintf(f, "[general]\n");
    fprintf(f, "start_on_logon = true\n");
    fprintf(f, "hide_on_startup = true\n");
    fprintf(f, "language = %s\n\n", system_language_code());

    fprintf(f, "[overlay]\n");
    fprintf(f, "# Position: top-left, top-center, top-right, center, bottom-left, bottom-center, bottom-right\n");
    fprintf(f, "position = bottom-right\n");
    fprintf(f, "margin_x = 24\n");
    fprintf(f, "margin_y = 52\n");
    fprintf(f, "duration_ms = 500\n");
    fprintf(f, "border_width = 2\n");
    fprintf(f, "sticky = false\n");
    fprintf(f, "dismiss_on_click = true\n\n");

    fprintf(f, "[colours]\n");
    fprintf(f, "# Exact colors matching Windows original cli_preview.png\n");
    fprintf(f, "background = #424242\n");
    fprintf(f, "border_activated = #4DB434\n");
    fprintf(f, "border_deactivated = #B4344D\n");
    fprintf(f, "text = #FFFFFF\n\n");

    fprintf(f, "[strings]\n");
    fprintf(f, "# Optional: custom OSD texts. Leave unset to use the selected language.\n");
    fprintf(f, "# text_caps_on = My custom text\n\n");

    fprintf(f, "[sound]\n");
    fprintf(f, "enable_sound = false\n");
    fprintf(f, "sound_on_cmd = \n");
    fprintf(f, "sound_off_cmd = \n");

    fclose(f);
    return true;
}

bool config_load(AppConfig *config, const char *custom_path) {
    if (!config) return false;
    config_set_defaults(config);

    const char *path = custom_path ? custom_path : config_get_default_path();

    if (access(path, F_OK) != 0) {
        config_save_default_if_missing(path);
        return true;
    }

    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    char current_section[64] = {0};

    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_whitespace(line);
        if (*trimmed == '#' || *trimmed == ';' || *trimmed == '\0') {
            continue;
        }

        if (*trimmed == '[') {
            char *end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
            }
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_whitespace(trimmed);
        char *val = trim_whitespace(eq + 1);

        // [icons] section
        if (strcasecmp(current_section, "icons") == 0) {
            if (strcasecmp(key, "num_lock") == 0) config->icon_num = parse_bool(val);
            else if (strcasecmp(key, "caps_lock") == 0) config->icon_caps = parse_bool(val);
            else if (strcasecmp(key, "scroll_lock") == 0) config->icon_scroll = parse_bool(val);
            else if (strcasecmp(key, "show_no_icons") == 0) config->show_no_icons = parse_bool(val);
            else if (strcasecmp(key, "tray_only_when_active") == 0 || strcasecmp(key, "always_show_when_active") == 0)
                config->tray_only_when_active = parse_bool(val);
        }
        // [notifications] section
        else if (strcasecmp(current_section, "notifications") == 0) {
            if (strcasecmp(key, "num_lock") == 0) config->notify_num = parse_bool(val);
            else if (strcasecmp(key, "caps_lock") == 0) config->notify_caps = parse_bool(val);
            else if (strcasecmp(key, "scroll_lock") == 0) config->notify_scroll = parse_bool(val);
            else if (strcasecmp(key, "show_no_notification") == 0) config->show_no_notification = parse_bool(val);
        }
        // [general]
        else if (strcasecmp(current_section, "general") == 0) {
            if (strcasecmp(key, "start_on_logon") == 0) config->start_on_logon = parse_bool(val);
            else if (strcasecmp(key, "hide_on_startup") == 0) config->hide_on_startup = parse_bool(val);
            else if (strcasecmp(key, "language") == 0) strncpy(config->lang, val, sizeof(config->lang) - 1);
        }
        // [overlay]
        else if (strcasecmp(current_section, "overlay") == 0) {
            if (strcasecmp(key, "position") == 0) config->position = config_parse_position(val);
            else if (strcasecmp(key, "margin_x") == 0) config->margin_x = atoi(val);
            else if (strcasecmp(key, "margin_y") == 0) config->margin_y = atoi(val);
            else if (strcasecmp(key, "duration_ms") == 0) config->duration_ms = atoi(val);
            else if (strcasecmp(key, "border_width") == 0) config->border_width = atoi(val);
            else if (strcasecmp(key, "sticky") == 0) config->sticky = parse_bool(val);
            else if (strcasecmp(key, "dismiss_on_click") == 0) config->dismiss_on_click = parse_bool(val);
            else if (strcasecmp(key, "persistent_caps_on") == 0) config->persistent_caps_on = parse_bool(val);
            else if (strcasecmp(key, "persistent_caps_off") == 0) config->persistent_caps_off = parse_bool(val);
            else if (strcasecmp(key, "persistent_num_on") == 0) config->persistent_num_on = parse_bool(val);
            else if (strcasecmp(key, "persistent_num_off") == 0) config->persistent_num_off = parse_bool(val);
            else if (strcasecmp(key, "persistent_scroll_on") == 0) config->persistent_scroll_on = parse_bool(val);
            else if (strcasecmp(key, "persistent_scroll_off") == 0) config->persistent_scroll_off = parse_bool(val);
        }
        // [colours] or [colors]
        else if (strcasecmp(current_section, "colours") == 0 || strcasecmp(current_section, "colors") == 0) {
            if (strcasecmp(key, "background") == 0 || strcasecmp(key, "bg_color") == 0)
                config->bg_color = parse_color(val, config->bg_color);
            else if (strcasecmp(key, "border_activated") == 0 || strcasecmp(key, "border_on") == 0)
                config->border_on = parse_color(val, config->border_on);
            else if (strcasecmp(key, "border_deactivated") == 0 || strcasecmp(key, "border_off") == 0)
                config->border_off = parse_color(val, config->border_off);
            else if (strcasecmp(key, "text") == 0 || strcasecmp(key, "text_color") == 0)
                config->text_color = parse_color(val, config->text_color);
        }
        // [strings]
        else if (strcasecmp(current_section, "strings") == 0) {
            if (strcasecmp(key, "text_caps_on") == 0) strncpy(config->text_caps_on, val, sizeof(config->text_caps_on) - 1);
            else if (strcasecmp(key, "text_caps_off") == 0) strncpy(config->text_caps_off, val, sizeof(config->text_caps_off) - 1);
            else if (strcasecmp(key, "text_num_on") == 0) strncpy(config->text_num_on, val, sizeof(config->text_num_on) - 1);
            else if (strcasecmp(key, "text_num_off") == 0) strncpy(config->text_num_off, val, sizeof(config->text_num_off) - 1);
            else if (strcasecmp(key, "text_scroll_on") == 0) strncpy(config->text_scroll_on, val, sizeof(config->text_scroll_on) - 1);
            else if (strcasecmp(key, "text_scroll_off") == 0) strncpy(config->text_scroll_off, val, sizeof(config->text_scroll_off) - 1);
        }
        // [sound]
        else if (strcasecmp(current_section, "sound") == 0) {
            if (strcasecmp(key, "enable_sound") == 0) config->enable_sound = parse_bool(val);
            else if (strcasecmp(key, "sound_on_cmd") == 0) strncpy(config->sound_on_cmd, val, sizeof(config->sound_on_cmd) - 1);
            else if (strcasecmp(key, "sound_off_cmd") == 0) strncpy(config->sound_off_cmd, val, sizeof(config->sound_off_cmd) - 1);
        }
    }

    fclose(f);
    apply_language_texts(config);
    return true;
}
