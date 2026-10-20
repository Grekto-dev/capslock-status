#include "sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void sound_play_alert(Display *d, const AppConfig *config, bool is_on) {
    if (!config || !config->enable_sound) return;

    const char *cmd = is_on ? config->sound_on_cmd : config->sound_off_cmd;
    if (cmd && cmd[0] != '\0') {
        pid_t pid = fork();
        if (pid == 0) {
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
            _exit(1);
        }
    } else if (d) {
        // Fallback to standard X11 audio bell
        XBell(d, is_on ? 30 : -30);
        XFlush(d);
    }
}
