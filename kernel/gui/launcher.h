/* ==============================================================================
 * Greenhouse OS - VERDANT Radial Launcher
 * ==============================================================================
 */

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <stdint.h>
#include "../input/input.h"

#define LAUNCHER_NODE_COUNT 6

#define LAUNCHER_APP_NONE     0
#define LAUNCHER_APP_TERMINAL 1
#define LAUNCHER_APP_FILES    2
#define LAUNCHER_APP_SYSMON   3
#define LAUNCHER_APP_BERRY    4
#define LAUNCHER_APP_CANVAS   5
#define LAUNCHER_APP_SETTINGS 6

typedef struct {
    int         app_id;
    char        key;            /* '1'..'6' */
    const char* title;
    const char* tag;
    const char* desc;
    uint32_t    accent;
    int         rel_x;          /* center-relative orbital offset */
    int         rel_y;
    int         w;
    int         h;
} launcher_node_t;

void launcher_init(void);
void launcher_show(void);
void launcher_hide(void);
void launcher_toggle(void);
int  launcher_is_visible(void);

void launcher_draw(int sw, int sh);
int  launcher_handle_event(const input_event_t* ev, int sw, int sh, int* out_launch_app);

#endif /* LAUNCHER_H */
