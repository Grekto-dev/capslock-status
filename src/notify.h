#ifndef NOTIFY_H
#define NOTIFY_H

#include "indicator.h"

void notify_send_alert(const AppConfig *config, LockKeyType key, bool is_on);

#endif // NOTIFY_H
