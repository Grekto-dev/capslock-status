#ifndef I18N_H
#define I18N_H

#include <stdbool.h>

// Texts used by the background daemon (OSD, tray tooltips and tray menu).
// OSD wording follows the translations of the original CapsLockIndicator.
typedef struct {
    const char *code;           // "en-GB", "pt-BR", ...
    const char *native_name;    // Name of the language in that language
    const char *key_name[3];    // Indexed by LockKeyType: num, caps, scroll
    const char *fmt_on;         // printf pattern with one %s (key name)
    const char *fmt_off;
    const char *menu_settings;
    const char *menu_quit;
} Lang;

extern const Lang I18N_LANGS[];
extern const int I18N_LANG_COUNT;

// Finds a language by code (case-insensitive; "pt" matches "pt-BR").
// Falls back to English when unknown.
const Lang *i18n_get(const char *code);

// Writes the "<key> is on/off" text for a lock key (index = LockKeyType) into dst.
void i18n_status_text(const Lang *lang, int key, bool is_on, char *dst, unsigned long dst_size);

// True if `text` is exactly one of the built-in OSD texts of any language. Used to tell
// user-customised texts (kept) apart from old auto-generated defaults (replaced by the language).
bool i18n_is_builtin_text(const char *text);

#endif // I18N_H
