/* ==============================================================================
 * Greenhouse OS - VERDANT System Rail
 * ==============================================================================
 */

#ifndef RAIL_H
#define RAIL_H

#include <stdint.h>
#include "surface.h"
#include "../input/input.h"

#define RAIL_HEIGHT 30

#define RAIL_ACTION_NONE     0
#define RAIL_ACTION_LAUNCHER 1
#define RAIL_ACTION_BERRY    2
#define RAIL_ACTION_EXIT     3
#define RAIL_ACTION_SURFACE  4

void rail_init(void);
void rail_draw(int sw, int sh, surface_t* surfaces, int count, int focused_id);
int  rail_handle_event(const input_event_t* ev, int sw, int sh,
                       surface_t* surfaces, int count,
                       int* out_action, uint32_t* out_surface_id);

#endif /* RAIL_H */
