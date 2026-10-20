#include "notify.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void notify_send_alert(const AppConfig *config, LockKeyType key, bool is_on) {
    if (!config) return;

    const char *status_str = indicator_get_status_text(config, key, is_on);

    pid_t pid = fork();
    if (pid == 0) {
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);

        char *const args[] = {
            "notify-send",
            "-a", "CapsLockIndicator",
            "-t", "1000",
            "-i", is_on ? "dialog-information" : "dialog-warning",
            (char *)status_str,
            NULL
        };
        execvp("notify-send", args);
        _exit(1);
    }
}
