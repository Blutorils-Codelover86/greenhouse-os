/* ==============================================================================
 * Greenhouse OS - VERDANT Settings / Display Inspector Surface
 * ==============================================================================
 */

#ifndef SETTINGS_SURFACE_H
#define SETTINGS_SURFACE_H

#include <stdint.h>
#include "surface.h"
#include "../input/input.h"

void settings_surface_init(void);
void settings_surface_draw(surface_t* s, int cx, int cy, int cw, int ch);
int  settings_surface_event(surface_t* s, const input_event_t* ev);

#endif /* SETTINGS_SURFACE_H */
