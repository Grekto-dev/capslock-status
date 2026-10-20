#ifndef CONFIG_H
#define CONFIG_H

#include "indicator.h"

void config_set_defaults(AppConfig *config);
bool config_load(AppConfig *config, const char *custom_path);
bool config_save_default_if_missing(const char *path);
const char *config_get_default_path(void);
OSDPosition config_parse_position(const char *str);
const char *config_position_to_string(OSDPosition pos);

#endif // CONFIG_H
