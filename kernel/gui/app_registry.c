/* ==============================================================================
 * Greenhouse OS - Light-Mode Application Registry (Implementation)
 * ==============================================================================
 */

#include "app_registry.h"
#include "gh_theme.h"
#include "verdant.h"
#include "../vfs.h"

static app_entry_t g_apps[APP_REGISTRY_MAX];
static int         g_app_count = 0;

static void app_strcpy(char* dst, int cap, const char* src) {
    int i = 0;
    if (src) {
        for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int app_strendswith(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    int slen = 0, ulen = 0;
    while (s[slen]) slen++;
    while (suffix[ulen]) ulen++;
    if (slen < ulen) return 0;
    for (int i = 0; i < ulen; i++) {
        char c1 = s[slen - ulen + i];
        char c2 = suffix[i];
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2) return 0;
    }
    return 1;
}

void app_registry_init(void) {
    g_app_count = 0;

    /* 1. Register Native Built-in Applications */
    app_registry_register_builtin(1, '1', "Terminal",       "TERM",  "Greenhouse interactive shell & CLI",        GH_COLOR_GREEN_LEAF_DEEP, verdant_open_terminal);
    app_registry_register_builtin(2, '2', "File Browser",   "FILES", "Browse FAT32 (C:) & RAMFS (R:) drives",     GH_COLOR_BLUE_INFO, verdant_open_files);
    app_registry_register_builtin(3, '3', "System Monitor", "SYS",   "Real-time CPU, RAM, heap, and processes",   GH_COLOR_AMBER_WARN, verdant_open_sysmon);
    app_registry_register_builtin(4, '4', "Berry AI",       "BERRY", "Interactive system assistant and companion", GH_COLOR_GREEN_LEAF, verdant_open_berry);
    app_registry_register_builtin(5, '5', "Canvas Studio",  "GFX",   "2D graphics demonstration and primitives",  GH_COLOR_GREEN_LEAF_DEEP, verdant_open_canvas);
    app_registry_register_builtin(6, '6', "System Info",    "INFO",  "Display properties & CPUID report",         GH_COLOR_GREEN_LEAF, verdant_open_settings);

    /* 2. Discover Userland Binaries on C:\ drive */
    vfs_node_t* c_root = vfs_resolve_path(NULL, "C:\\");
    if (c_root) {
        vfs_dirent_t ent;
        for (uint32_t i = 0; i < 32 && g_app_count < APP_REGISTRY_MAX; i++) {
            if (vfs_readdir(c_root, i, &ent) != 0) break;
            if (!ent.is_dir && app_strendswith(ent.name, ".ELF")) {
                /* Form full path */
                char full_path[64];
                full_path[0] = 'C';
                full_path[1] = ':';
                full_path[2] = '\\';
                app_strcpy(full_path + 3, sizeof(full_path) - 3, ent.name);

                char key = (char)('7' + (g_app_count - 6));
                if (key > '9') key = '*';

                app_registry_register_elf(g_app_count + 1, key, ent.name, "ELF",
                                          "Ring 3 Userland ELF executable",
                                          GH_COLOR_GREEN_LEAF_DEEP, full_path);
            }
        }
    }
}

int app_registry_register_builtin(int id, char key, const char* name, const char* tag,
                                  const char* desc, uint32_t accent, app_launch_fn launch_fn) {
    if (g_app_count >= APP_REGISTRY_MAX) return -1;
    app_entry_t* e = &g_apps[g_app_count++];
    e->id = id;
    e->key = key;
    app_strcpy(e->name, sizeof(e->name), name);
    app_strcpy(e->tag, sizeof(e->tag), tag);
    app_strcpy(e->desc, sizeof(e->desc), desc);
    e->accent = accent;
    e->type = APP_TYPE_BUILTIN;
    e->path[0] = '\0';
    e->launch_fn = launch_fn;
    return 0;
}

int app_registry_register_elf(int id, char key, const char* name, const char* tag,
                              const char* desc, uint32_t accent, const char* path) {
    if (g_app_count >= APP_REGISTRY_MAX) return -1;
    app_entry_t* e = &g_apps[g_app_count++];
    e->id = id;
    e->key = key;
    app_strcpy(e->name, sizeof(e->name), name);
    app_strcpy(e->tag, sizeof(e->tag), tag);
    app_strcpy(e->desc, sizeof(e->desc), desc);
    e->accent = accent;
    e->type = APP_TYPE_USERLAND_ELF;
    app_strcpy(e->path, sizeof(e->path), path);
    e->launch_fn = 0;
    return 0;
}

int app_registry_count(void) {
    return g_app_count;
}

const app_entry_t* app_registry_get(int index) {
    if (index < 0 || index >= g_app_count) return NULL;
    return &g_apps[index];
}

const app_entry_t* app_registry_find_by_id(int id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i].id == id) return &g_apps[i];
    }
    return NULL;
}

const app_entry_t* app_registry_find_by_key(char key) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i].key == key) return &g_apps[i];
    }
    return NULL;
}

int app_registry_launch(int id) {
    const app_entry_t* app = app_registry_find_by_id(id);
    if (!app) return -1;

    if (app->type == APP_TYPE_BUILTIN) {
        if (app->launch_fn) return app->launch_fn();
        return 0;
    } else if (app->type == APP_TYPE_USERLAND_ELF) {
        return verdant_open_terminal_run(app->path);
    }
    return -1;
}