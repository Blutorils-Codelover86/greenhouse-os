/* ==============================================================================
 * Greenhouse OS - VERDANT Application Registry
 *
 * Provides a central registry for all available Greenhouse applications,
 * including native built-in surfaces and userland ELF binaries discovered on
 * persistent storage (C:\) or RAMFS (R:\).
 * ==============================================================================
 */

#ifndef APP_REGISTRY_H
#define APP_REGISTRY_H

#include <stdint.h>
#include <stddef.h>

#define APP_REGISTRY_MAX 16

typedef enum {
    APP_TYPE_BUILTIN = 0,
    APP_TYPE_USERLAND_ELF = 1,
} app_type_t;

typedef int (*app_launch_fn)(void);

typedef struct {
    int           id;
    char          key;            /* Shortcut key '1'..'8' */
    char          name[32];       /* Application Title */
    char          tag[12];        /* Short badge tag e.g. "TERM", "FILES", "BERRY" */
    char          desc[64];       /* Description shown in launcher */
    uint32_t      accent;         /* Color accent */
    app_type_t    type;           /* Built-in or Userland ELF */
    char          path[64];       /* Path to executable (if ELF) */
    app_launch_fn launch_fn;      /* Launch callback (if built-in) */
} app_entry_t;

void app_registry_init(void);
int  app_registry_register_builtin(int id, char key, const char* name, const char* tag,
                                   const char* desc, uint32_t accent, app_launch_fn launch_fn);
int  app_registry_register_elf(int id, char key, const char* name, const char* tag,
                               const char* desc, uint32_t accent, const char* path);
int  app_registry_count(void);
const app_entry_t* app_registry_get(int index);
const app_entry_t* app_registry_find_by_id(int id);
const app_entry_t* app_registry_find_by_key(char key);
int  app_registry_launch(int id);

#endif /* APP_REGISTRY_H */
