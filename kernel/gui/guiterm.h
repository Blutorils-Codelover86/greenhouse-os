/* ==============================================================================
 * Greenhouse OS - Phase 7: Graphical Terminal
 *
 * A window that behaves like a real terminal: a scrolling text buffer drawn
 * with the built in 8x16 font, line editing with the keyboard (backspace,
 * arrows for history, enter), and a small built in command set that reports
 * live kernel state instead of fake data.
 * ==============================================================================
 */

#ifndef GUITERM_H
#define GUITERM_H

#include <stdint.h>
#include "../input/input.h"

#define GTERM_LINES     120
#define GTERM_COLS      78
#define GTERM_HISTORY   16
#define GTERM_PAD       4

typedef struct guiterm {
    char  lines[GTERM_LINES][GTERM_COLS + 1];
    int   count;                      /* lines used in the buffer            */
    int   scroll;                     /* first visible line                  */

    char  history[GTERM_HISTORY][GTERM_COLS + 1];
    int   history_count;
    int   history_pos;                /* -1 = editing a fresh line           */

    char  input[GTERM_COLS + 1];
    int   input_len;

    uint32_t fg;
    uint32_t bg;
    uint32_t prompt_color;
    uint32_t accent;
    uint64_t commands_run;
} guiterm_t;

void guiterm_init(guiterm_t* t);
void guiterm_write_line(guiterm_t* t, const char* text);
void guiterm_printf(guiterm_t* t, const char* text);
void guiterm_clear(guiterm_t* t);
void guiterm_handle_event(guiterm_t* t, const input_event_t* ev);
void guiterm_draw(guiterm_t* t, int cx, int cy, int cw, int ch);
void guiterm_execute(guiterm_t* t, const char* line);

#endif /* GUITERM_H */
