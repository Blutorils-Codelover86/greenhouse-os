/* ==============================================================================
 * Greenhouse OS - VERDANT Canvas / Graphics Demo Surface
 * ==============================================================================
 */

#ifndef CANVAS_SURFACE_H
#define CANVAS_SURFACE_H

#include <stdint.h>
#include "surface.h"
#include "../input/input.h"

void canvas_surface_init(void);
void canvas_surface_draw(surface_t* s, int cx, int cy, int cw, int ch);
int  canvas_surface_event(surface_t* s, const input_event_t* ev);

#endif /* CANVAS_SURFACE_H */
