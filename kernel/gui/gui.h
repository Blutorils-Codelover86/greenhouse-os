/* ==============================================================================
 * Greenhouse OS - Phase 7: Graphical User Interface
 *
 * The shell entry point of the windowing system:
 *   gui_enter()   - leave the text console, program a graphics mode, create
 *                   the window manager state and the sample windows,
 *   gui_run()     - the event loop (input queue -> window manager -> compose ->
 *                   cursor -> present), which returns to the caller when the
 *                   user asks to leave the GUI,
 *   gui_leave()   - release the back buffer and restore the VGA text console.
 *
 * The whole GUI is optional: if no graphics mode can be programmed the text
 * shell keeps working exactly as before.
 * ==============================================================================
 */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "window.h"

#define GUI_EXIT_NONE      0
#define GUI_EXIT_ESC       1
#define GUI_EXIT_CLOSED    2
#define GUI_EXIT_TIMEOUT   3
#define GUI_EXIT_ERROR     4

typedef struct {
    int      width;
    int      height;
    int      bpp;
    int      backend_multiboot;
    int      back_buffer;
    uint64_t back_buffer_size;
    uint32_t presents;
    int      frames;
    int      events_processed;
    int      exit_reason;
} gui_report_t;

void gui_init(void);
int  gui_enter(void);
int  gui_run(int max_seconds);
void gui_leave(void);

int  gui_is_running(void);
void gui_request_exit(int reason);
int  gui_get_exit_reason(void);
void gui_get_report(gui_report_t* out);
int  gui_open_terminal(void);
int  gui_open_about(void);

#endif /* GUI_H */
