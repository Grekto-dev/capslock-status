#ifndef XAPP_API_H
#define XAPP_API_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>

typedef struct {
    void *libxapp;
    void *libglib;
    void *libgobject;

    void* (*icon_new_with_name)(const char *);
    void (*set_icon_name)(void *, const char *);
    void (*set_tooltip_text)(void *, const char *);
    void (*set_visible)(void *, bool);
    bool (*g_main_context_iteration)(void *, bool);
    void (*g_set_prgname)(const char *);
    unsigned long (*g_signal_connect_data)(void *, const char *, void *, void *, void *, int);
    void (*g_object_unref)(void *);

    bool available;
} XAppAPI;

static inline bool xapp_api_load(XAppAPI *api) {
    if (!api) return false;
    memset(api, 0, sizeof(*api));

    api->libxapp = dlopen("libxapp.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!api->libxapp) {
        api->libxapp = dlopen("libxapp.so", RTLD_LAZY | RTLD_GLOBAL);
    }
    if (!api->libxapp) return false;

    api->libglib = dlopen("libglib-2.0.so.0", RTLD_LAZY | RTLD_GLOBAL);
    if (!api->libglib) {
        api->libglib = dlopen("libglib-2.0.so", RTLD_LAZY | RTLD_GLOBAL);
    }
    if (!api->libglib) return false;

    api->libgobject = dlopen("libgobject-2.0.so.0", RTLD_LAZY | RTLD_GLOBAL);
    if (!api->libgobject) {
        api->libgobject = dlopen("libgobject-2.0.so", RTLD_LAZY | RTLD_GLOBAL);
    }
    if (!api->libgobject) return false;

    api->icon_new_with_name = (void* (*)(const char *))dlsym(api->libxapp, "xapp_status_icon_new_with_name");
    api->set_icon_name = (void (*)(void *, const char *))dlsym(api->libxapp, "xapp_status_icon_set_icon_name");
    api->set_tooltip_text = (void (*)(void *, const char *))dlsym(api->libxapp, "xapp_status_icon_set_tooltip_text");
    api->set_visible = (void (*)(void *, bool))dlsym(api->libxapp, "xapp_status_icon_set_visible");

    api->g_main_context_iteration = (bool (*)(void *, bool))dlsym(api->libglib, "g_main_context_iteration");
    api->g_set_prgname = (void (*)(const char *))dlsym(api->libglib, "g_set_prgname");
    api->g_signal_connect_data = (unsigned long (*)(void *, const char *, void *, void *, void *, int))dlsym(api->libgobject, "g_signal_connect_data");
    api->g_object_unref = (void (*)(void *))dlsym(api->libgobject, "g_object_unref");

    if (api->icon_new_with_name && api->set_icon_name && api->set_tooltip_text &&
        api->set_visible && api->g_main_context_iteration && api->g_signal_connect_data) {
        api->available = true;
        if (api->g_set_prgname) {
            api->g_set_prgname("capslock-status");
        }
        return true;
    }

    return false;
}

#endif // XAPP_API_H
