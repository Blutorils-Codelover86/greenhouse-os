/* ==============================================================================
 * Greenhouse OS — Modern Desktop Shell (Header)
 * ==============================================================================
 * Manages the top floating status bar, bottom application dock, and workspace
 * window management.
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
#define SHELL_TOPBAR_H       30
#define SHELL_TOPBAR_PAD     12

/* Dock Geometry */
#define SHELL_DOCK_Y         714
#define SHELL_DOCK_H         44
#define SHELL_DOCK_ITEMS     6

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
int  shell_topbar_hit_test(int x, int y, int* out_tab_index);
int  shell_dock_hit_test(int x, int y);

/* Get dock item metadata */
const shell_dock_item_t* shell_get_dock_item(int index);

#endif /* GUI_SHELL_H */
