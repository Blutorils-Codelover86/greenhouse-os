/* ==============================================================================
 * Greenhouse OS — Modern Desktop Shell (Header)
 * ==============================================================================
 * Combines macOS menubar elegance with Windows taskbar & system tray
 * practicality into a refined botanical Greenhouse interface.
 * ==============================================================================
 */

#ifndef GUI_SHELL_H
#define GUI_SHELL_H

#include <stdint.h>
#include <stddef.h>
#include "surface.h"
#include "../input/input.h"

/* Top Bar Geometry */
#define SHELL_TOPBAR_Y       8
#define SHELL_TOPBAR_H       32
#define SHELL_TOPBAR_PAD     12

/* Dock Geometry */
#define SHELL_DOCK_Y         712
#define SHELL_DOCK_H         48
#define SHELL_DOCK_ITEMS     6

/* Shell Topbar Hit Test Codes */
#define SHELL_HIT_NONE       0
#define SHELL_HIT_BACKGROUND 1
#define SHELL_HIT_TAB        2
#define SHELL_HIT_SYSMON     997
#define SHELL_HIT_BRAND      998
#define SHELL_HIT_CONSOLE    999

typedef struct {
    const char* name;
    const char* icon;
    const char* shortcut;
    uint32_t    color;
    surface_id_t surface_id;
} shell_dock_item_t;

void shell_init(void);

/* Render the top status bar & bottom dock */
void shell_draw_topbar(surface_t* active_surface, surface_t** all_surfaces, int surface_count);
void shell_draw_dock(int hover_item, surface_t** all_surfaces, int surface_count);

/* Hit testing for shell elements */
int  shell_topbar_hit_test(int x, int y, surface_t** all_surfaces, int surface_count, int* out_tab_index);
int  shell_dock_hit_test(int x, int y);

/* Get dock item metadata */
const shell_dock_item_t* shell_get_dock_item(int index);

#endif /* GUI_SHELL_H */
