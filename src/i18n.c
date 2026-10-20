#include "i18n.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

// key_name order: { Num, Caps, Scroll } (matches LockKeyType).
const Lang I18N_LANGS[] = {
    { "en-GB", "English",
      { "Num lock", "Caps lock", "Scroll lock" },
      "%s is on", "%s is off", "Settings", "Quit" },
    { "pt-BR", "Português (Brasil)",
      { "Num lock", "Caps lock", "Scroll lock" },
      "%s ativado", "%s desativado", "Configurações", "Sair" },
    { "es", "Español",
      { "Números", "Mayúsculas", "Bloq. Despl" },
      "%s activado", "%s desactivado", "Configuración", "Salir" },
    { "fr", "Français",
      { "Verrouillage numérique", "Verrouillage majuscules", "Arrêt défil." },
      "%s actif", "%s inactif", "Paramètres", "Quitter" },
    { "de", "Deutsch",
      { "Num-Lock", "Feststelltaste", "Rollen" },
      "%s ist an", "%s ist aus", "Einstellungen", "Beenden" },
    { "it", "Italiano",
      { "Blocco numerico", "Blocco maiuscole", "Blocco scorrimento" },
      "%s è attivato", "%s è disattivato", "Impostazioni", "Esci" },
};

const int I18N_LANG_COUNT = (int)(sizeof(I18N_LANGS) / sizeof(I18N_LANGS[0]));

const Lang *i18n_get(const char *code) {
    if (code && *code) {
        for (int i = 0; i < I18N_LANG_COUNT; i++) {
            if (strcasecmp(code, I18N_LANGS[i].code) == 0) return &I18N_LANGS[i];
        }
        // Prefix match: "pt" -> "pt-BR", "en-US" -> "en-GB"
        for (int i = 0; i < I18N_LANG_COUNT; i++) {
            size_t n = 2;
            if (strncasecmp(code, I18N_LANGS[i].code, n) == 0) return &I18N_LANGS[i];
        }
    }
    return &I18N_LANGS[0];
}

void i18n_status_text(const Lang *lang, int key, bool is_on, char *dst, unsigned long dst_size) {
    if (!lang || key < 0 || key > 2 || !dst || dst_size == 0) return;
    snprintf(dst, dst_size, is_on ? lang->fmt_on : lang->fmt_off, lang->key_name[key]);
}

bool i18n_is_builtin_text(const char *text) {
    if (!text || !*text) return true;
    char buf[128];
    for (int l = 0; l < I18N_LANG_COUNT; l++) {
        for (int k = 0; k < 3; k++) {
            for (int on = 0; on < 2; on++) {
                i18n_status_text(&I18N_LANGS[l], k, on, buf, sizeof(buf));
                if (strcmp(buf, text) == 0) return true;
            }
        }
    }
    return false;
}
