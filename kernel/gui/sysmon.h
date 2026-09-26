/* ==============================================================================
 * Greenhouse OS - VERDANT System Monitor
 * ==============================================================================
 */

#ifndef SYSMON_H
#define SYSMON_H

#include <stdint.h>
#include "surface.h"
#include "../input/input.h"

void sysmon_init(void);
void sysmon_draw(surface_t* s, int cx, int cy, int cw, int ch);
int  sysmon_event(surface_t* s, const input_event_t* ev);

#endif /* SYSMON_H */
