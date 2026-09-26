/* ==============================================================================
 * Greenhouse OS - VERDANT Berry Assistant Surface
 * ==============================================================================
 */

#ifndef BERRY_SURFACE_H
#define BERRY_SURFACE_H

#include <stdint.h>
#include "surface.h"
#include "../input/input.h"

#define BERRY_CHAT_LINES 16
#define BERRY_LINE_LEN   72

typedef struct {
    char  lines[BERRY_CHAT_LINES][BERRY_LINE_LEN + 1];
    uint32_t colors[BERRY_CHAT_LINES];
    int   line_count;

    char  input[BERRY_LINE_LEN + 1];
    int   input_len;

    int   requested_action; /* e.g. open terminal, files, etc. */
} berry_surface_state_t;

void berry_surface_init(berry_surface_state_t* st);
void berry_surface_draw(surface_t* s, int cx, int cy, int cw, int ch);
int  berry_surface_event(surface_t* s, const input_event_t* ev);

#endif /* BERRY_SURFACE_H */
