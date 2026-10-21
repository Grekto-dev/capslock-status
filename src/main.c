#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <locale.h>
#include <X11/Xlib.h>

#include "indicator.h"
#include "config.h"
#include "osd.h"
#include "tray.h"
#include "notify.h"
#include "sound.h"
#include "i18n.h"

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_reload = 0;

static int x_error_handler(Display *d, XErrorEvent *e) {
    (void)d;
    (void)e;
    // Suppress benign X11 errors (e.g. BadWindow if window was reparented/closed by window manager)
    return 0;
}

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = 0;
    } else if (sig == SIGHUP) {
        g_reload = 1;
    } else if (sig == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }
}

static void launch_gui(void) {
    pid_t pid = fork();
    if (pid == 0) {
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);

        char exe_path[1024] = {0};
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len > 0) {
            exe_path[len] = '\0';
            char *slash = strrchr(exe_path, '/');
            if (slash) *slash = '\0';
        } else {
            strcpy(exe_path, ".");
        }

        char gui_script[1024];
        snprintf(gui_script, sizeof(gui_script), "%s/capslock-status-gui", exe_path);
        execlp(gui_script, gui_script, (char *)NULL);

        snprintf(gui_script, sizeof(gui_script), "%s/capslock-indicator-gui", exe_path);
        execlp(gui_script, gui_script, (char *)NULL);

        char py_script[1024];
        snprintf(py_script, sizeof(py_script), "%s/src/gui.py", exe_path);
        execlp("python3", "python3", py_script, (char *)NULL);
        _exit(1);
    }
}

static void launch_menu(int x, int y) {
    FILE *f = fopen("/tmp/capslock_click.log", "a");
    if (f) {
        fprintf(f, "LAUNCH_MENU called with x=%d y=%d\n", x, y);
        fclose(f);
    }
    pid_t pid = fork();
    if (pid == 0) {
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);

        char sx[32], sy[32];
        snprintf(sx, sizeof(sx), "%d", x);
        snprintf(sy, sizeof(sy), "%d", y);

        char exe_path[1024] = {0};
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len > 0) {
            exe_path[len] = '\0';
            char *slash = strrchr(exe_path, '/');
            if (slash) *slash = '\0';
        } else {
            strcpy(exe_path, ".");
        }

        char gui_script[1024];
        snprintf(gui_script, sizeof(gui_script), "%s/capslock-status-gui", exe_path);
        execlp(gui_script, gui_script, "--menu", sx, sy, (char *)NULL);

        snprintf(gui_script, sizeof(gui_script), "%s/capslock-indicator-gui", exe_path);
        execlp(gui_script, gui_script, "--menu", sx, sy, (char *)NULL);

        char py_script[1024];
        snprintf(py_script, sizeof(py_script), "%s/src/gui.py", exe_path);
        execlp("python3", "python3", py_script, "--menu", sx, sy, (char *)NULL);
        _exit(1);
    }
}

static void print_usage(const char *prog_name) {
    printf("Uso: %s [OPÇÕES]\n\n", prog_name);
    printf("CapsLock Status v1.0.0 (Port nativo e ultra-leve para Linux)\n");
    printf("Baseado no projeto original CapsLockIndicator de Jonas Kohl.\n");
    printf("Projetado para consumo irrisório de CPU/RAM (< 3 MB RSS) e hardware modesto.\n\n");
    printf("Opções principais:\n");
    printf("  -d, --daemon             Executa o programa em segundo plano (daemon)\n");
    printf("  -g, --gui                Abre a janela gráfica de configurações\n");
    printf("  -t, --tray               Ativa os ícones na bandeja do sistema [N] [C] [S]\n");
    printf("  -s, --status             Exibe o estado atual das teclas (Caps/Num/Scroll) e sai\n");
    printf("      --json               Formata a saída de --status como JSON\n");
    printf("      --test               Exibe um aviso de teste na tela e sai\n");
    printf("      --toggle KEY         Alterna o estado de uma tecla (caps, num, scroll) e sai\n");
    printf("\nOpções do OSD (Aviso na tela):\n");
    printf("      --no-osd             Desativa o aviso visual na tela\n");
    printf("      --position POS       Posição: bottom-right, bottom-center, top-right, center, etc.\n");
    printf("      --duration MS        Tempo de exibição do aviso em milissegundos (padrão: 1200)\n");
    printf("      --sticky             Mantém o aviso visível na tela enquanto a tecla estiver ativada\n");
    printf("\nIntegração e Autoinício:\n");
    printf("      --install-autostart   Cria entrada para iniciar automaticamente com a sessão\n");
    printf("      --uninstall-autostart Remove a entrada de autoinício da sessão\n");
    printf("  -c, --config PATH        Especifica um caminho alternativo para o config.ini\n");
    printf("  -v, --version            Exibe a versão do programa\n");
    printf("  -h, --help               Exibe esta mensagem de ajuda\n\n");
    printf("Arquivo de configuração: %s\n", config_get_default_path());
}

static bool install_autostart(void) {
    const char *home = getenv("HOME");
    if (!home) return false;

    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.config/autostart", home);
    mkdir(dir, 0755);

    char filepath[2048];
    snprintf(filepath, sizeof(filepath), "%s/capslock-status.desktop", dir);

    char exe_path[1024] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) {
        strncpy(exe_path, "capslock-status", sizeof(exe_path) - 1);
    } else {
        exe_path[len] = '\0';
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "Erro ao criar arquivo de autoinício em %s\n", filepath);
        return false;
    }

    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Name=CapsLock Status\n");
    fprintf(f, "Comment=Status de Caps Lock, Num Lock e Scroll Lock\n");
    fprintf(f, "Exec=%s -d\n", exe_path);
    fprintf(f, "Icon=capslock-status\n");
    fprintf(f, "Terminal=false\n");
    fprintf(f, "Categories=Utility;\n");
    fprintf(f, "X-GNOME-Autostart-enabled=true\n");

    fclose(f);
    printf("Autoinício instalado com sucesso em:\n  %s\n", filepath);
    return true;
}

static bool uninstall_autostart(void) {
    const char *home = getenv("HOME");
    if (!home) return false;

    char filepath[2048];
    snprintf(filepath, sizeof(filepath), "%s/.config/autostart/capslock-status.desktop", home);
    char legacy[2048];
    snprintf(legacy, sizeof(legacy), "%s/.config/autostart/capslock-indicator.desktop", home);
    unlink(legacy);

    if (unlink(filepath) == 0) {
        printf("Autoinício removido com sucesso: %s\n", filepath);
        return true;
    } else {
        printf("Arquivo de autoinício não encontrado: %s\n", filepath);
        return false;
    }
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    const char *config_path = NULL;
    bool daemon_mode = false;
    bool status_mode = false;
    bool json_mode = false;
    bool test_mode = false;
    bool open_gui_mode = false;
    char *toggle_target = NULL;

    int opt_tray = -1;
    int opt_osd = -1;
    int opt_sticky = -1;
    int opt_duration = -1;
    char *opt_position = NULL;

    static struct option long_options[] = {
        {"help",                no_argument,       0, 'h'},
        {"version",             no_argument,       0, 'v'},
        {"daemon",              no_argument,       0, 'd'},
        {"gui",                 no_argument,       0, 'g'},
        {"tray",                no_argument,       0, 't'},
        {"status",              no_argument,       0, 's'},
        {"json",                no_argument,       0, 'j'},
        {"test",                no_argument,       0, 1001},
        {"toggle",              required_argument, 0, 1002},
        {"no-osd",              no_argument,       0, 1003},
        {"sticky",              no_argument,       0, 1004},
        {"duration",            required_argument, 0, 1005},
        {"position",            required_argument, 0, 1006},
        {"install-autostart",   no_argument,       0, 1011},
        {"uninstall-autostart", no_argument,       0, 1012},
        {"config",              required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvdgtsjc:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                printf("%s versão %s (Linux Port)\n", APP_NAME, APP_VERSION);
                return 0;
            case 'd':
                daemon_mode = true;
                break;
            case 'g':
                open_gui_mode = true;
                break;
            case 't':
                opt_tray = 1;
                break;
            case 's':
                status_mode = true;
                break;
            case 'j':
                json_mode = true;
                break;
            case 1001: // --test
                test_mode = true;
                break;
            case 1002: // --toggle
                toggle_target = optarg;
                break;
            case 1003: // --no-osd
                opt_osd = 0;
                break;
            case 1004: // --sticky
                opt_sticky = 1;
                break;
            case 1005: // --duration
                opt_duration = atoi(optarg);
                break;
            case 1006: // --position
                opt_position = optarg;
                break;
            case 1011: // --install-autostart
                return install_autostart() ? 0 : 1;
            case 1012: // --uninstall-autostart
                return uninstall_autostart() ? 0 : 1;
            case 'c':
                config_path = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (open_gui_mode) {
        launch_gui();
        return 0;
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Erro: Não foi possível conectar ao servidor X11 (DISPLAY=%s).\n", getenv("DISPLAY"));
        return 1;
    }

    // Set X error handler to prevent crashing on asynchronous X errors
    XSetErrorHandler(x_error_handler);

    AppState app;
    if (!indicator_init(&app, display)) {
        XCloseDisplay(display);
        return 1;
    }

    // Handle --status
    if (status_mode) {
        if (json_mode) {
            printf("{\"num_lock\": %s, \"caps_lock\": %s, \"scroll_lock\": %s}\n",
                   app.state[KEY_NUM_LOCK] ? "true" : "false",
                   app.state[KEY_CAPS_LOCK] ? "true" : "false",
                   app.state[KEY_SCROLL_LOCK] ? "true" : "false");
        } else {
            printf("Num Lock:    [%s]\n", app.state[KEY_NUM_LOCK] ? "ATIVADO" : "DESATIVADO");
            printf("Caps Lock:   [%s]\n", app.state[KEY_CAPS_LOCK] ? "ATIVADO" : "DESATIVADO");
            printf("Scroll Lock: [%s]\n", app.state[KEY_SCROLL_LOCK] ? "ATIVADO" : "DESATIVADO");
        }
        XCloseDisplay(display);
        return 0;
    }

    // Handle --toggle
    if (toggle_target) {
        LockKeyType k = KEY_CAPS_LOCK;
        if (strcasecmp(toggle_target, "caps") == 0 || strcasecmp(toggle_target, "capslock") == 0) {
            k = KEY_CAPS_LOCK;
        } else if (strcasecmp(toggle_target, "num") == 0 || strcasecmp(toggle_target, "numlock") == 0) {
            k = KEY_NUM_LOCK;
        } else if (strcasecmp(toggle_target, "scroll") == 0 || strcasecmp(toggle_target, "scrolllock") == 0) {
            k = KEY_SCROLL_LOCK;
        } else {
            fprintf(stderr, "Tecla desconhecida: %s (opções válidas: caps, num, scroll)\n", toggle_target);
            XCloseDisplay(display);
            return 1;
        }
        indicator_toggle(&app, k);
        printf("%s agora está: [%s]\n",
               (k == KEY_CAPS_LOCK) ? "Caps Lock" : (k == KEY_NUM_LOCK) ? "Num Lock" : "Scroll Lock",
               app.state[k] ? "ATIVADO" : "DESATIVADO");
        XCloseDisplay(display);
        return 0;
    }

    // Load configuration
    AppConfig config;
    config_load(&config, config_path);

    // Apply CLI overrides
    if (opt_tray == 1) {
        config.show_no_icons = false;
    }
    if (opt_osd != -1) config.show_osd = (opt_osd == 1);
    if (opt_sticky != -1) config.sticky = (opt_sticky == 1);
    if (opt_duration != -1) config.duration_ms = opt_duration;
    if (opt_position) config.position = config_parse_position(opt_position);

    // Handle --test
    if (test_mode) {
        OSDContext osd_test;
        if (osd_init(&osd_test, display, app.screen, &config)) {
            printf("Exibindo aviso OSD com animação suave de fade out...\n");
            osd_show(&osd_test, &config, KEY_CAPS_LOCK, true);
            while (osd_is_visible(&osd_test)) {
                long rem = osd_get_remaining_ms(&osd_test);
                if (rem <= 0) {
                    osd_hide(&osd_test);
                    break;
                }
                usleep((rem > 20 ? 20 : rem) * 1000);
                osd_update_animation(&osd_test, &config);
            }
            osd_destroy(&osd_test);
        }
        XCloseDisplay(display);
        return 0;
    }

    // Daemonize if requested
    if (daemon_mode) {
        if (daemon(0, 0) != 0) {
            perror("daemon");
            XCloseDisplay(display);
            return 1;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    OSDContext osd;
    bool osd_ready = false;
    if (config.show_osd && !config.show_no_notification) {
        osd_ready = osd_init(&osd, display, app.screen, &config);
    }

    TrayContext tray;
    bool tray_ready = false;
    tray_ready = tray_init(&tray, display, app.screen, &config);
    if (tray_ready) {
        tray_update(&tray, &config, &app);
    }

    bool prev_state[KEY_COUNT];
    for (int i = 0; i < KEY_COUNT; i++) {
        prev_state[i] = app.state[i];
    }

    int x11_fd = ConnectionNumber(display);

    while (g_running) {
        if (g_reload) {
            g_reload = 0;
            config_load(&config, config_path);
            if (osd_ready) {
                osd_destroy(&osd);
                osd_ready = osd_init(&osd, display, app.screen, &config);
            }
            if (tray_ready) {
                tray_update(&tray, &config, &app);
            }
        }

        while (XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);

            // Keyboard indicator state change (Instant 0ms response via direct XkbStateNotify)
            if (ev.type == app.xkb_event_code) {
                XkbEvent *xev = (XkbEvent *)&ev;
                if (xev->any.xkb_type == XkbStateNotify) {
                    app.state[KEY_CAPS_LOCK] = (xev->state.locked_mods & app.caps_mask) != 0;
                    app.state[KEY_NUM_LOCK] = (xev->state.locked_mods & app.num_mask) != 0;
                    if (app.scroll_mask) {
                        app.state[KEY_SCROLL_LOCK] = (xev->state.locked_mods & app.scroll_mask) != 0;
                    }
                } else if (xev->any.xkb_type == XkbIndicatorStateNotify) {
                    bool ind_scroll = (xev->indicators.state & (1U << app.xkb_indices[KEY_SCROLL_LOCK])) != 0;
                    app.state[KEY_SCROLL_LOCK] = ind_scroll;
                } else {
                    indicator_query_state(&app);
                }

                for (int k = 0; k < KEY_COUNT; k++) {
                    if (app.state[k] != prev_state[k]) {
                        prev_state[k] = app.state[k];

                        bool notify_enabled = false;
                        if (!config.show_no_notification && config.show_osd) {
                            if (k == KEY_NUM_LOCK && config.notify_num) notify_enabled = true;
                            if (k == KEY_CAPS_LOCK && config.notify_caps) notify_enabled = true;
                            if (k == KEY_SCROLL_LOCK && config.notify_scroll) notify_enabled = true;
                        }

                        if (notify_enabled && osd_ready) {
                            osd_show(&osd, &config, (LockKeyType)k, app.state[k]);
                            XFlush(display);
                        }

                        if (tray_ready) {
                            tray_update(&tray, &config, &app);
                        }

                        if (config.enable_sound) {
                            sound_play_alert(display, &config, app.state[k]);
                        }
                    }
                }
            }

            if (osd_ready) {
                osd_handle_event(&osd, &config, &ev);
            }
            if (tray_ready) {
                tray_iterate(&tray);
                if (tray.action_open_gui) {
                    tray.action_open_gui = false;
                    launch_gui();
                }
                if (tray.action_show_menu) {
                    tray.action_show_menu = false;
                    launch_menu(tray.action_menu_x, tray.action_menu_y);
                }

                bool open_gui = false;
                bool show_menu = false;
                int menu_x = 0, menu_y = 0;
                tray_handle_event(&tray, &config, &app, &ev, &open_gui, &show_menu, &menu_x, &menu_y);
                if (open_gui) {
                    launch_gui();
                }
                if (show_menu) {
                    launch_menu(menu_x, menu_y);
                }
            }
        }

        if (!g_running) break;

        // Calculate timeout AFTER all X events have been processed
        int timeout_ms = -1;
        if (osd_ready && osd_is_visible(&osd)) {
            long remaining = osd_get_remaining_ms(&osd);
            if (remaining <= 0) {
                osd_hide(&osd);
                timeout_ms = -1;
            } else {
                timeout_ms = (int)remaining;
            }
        }

        if (tray_ready && tray.has_xapp) {
            if (timeout_ms < 0 || timeout_ms > 50) {
                timeout_ms = 50;
            }
        }

        struct pollfd pfd;
        pfd.fd = x11_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        poll(&pfd, 1, timeout_ms);

        if (tray_ready) {
            tray_iterate(&tray);
            if (tray.action_open_gui) {
                tray.action_open_gui = false;
                launch_gui();
            }
            if (tray.action_show_menu) {
                tray.action_show_menu = false;
                launch_menu(tray.action_menu_x, tray.action_menu_y);
            }
        }

        if (osd_ready && osd_is_visible(&osd)) {
            osd_update_animation(&osd, &config);
        }
    }

    if (osd_ready) {
        osd_hide(&osd);
        osd_destroy(&osd);
    }
    if (tray_ready) {
        tray_destroy(&tray);
    }
    XCloseDisplay(display);

    return 0;
}
