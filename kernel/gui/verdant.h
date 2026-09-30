/* ==============================================================================
 * Greenhouse OS - VERDANT Graphical Environment
 * ==============================================================================
 */

#ifndef VERDANT_H
#define VERDANT_H

#include <stdint.h>
#include "surface.h"
#include "compositor.h"
#include "launcher.h"
#include "rail.h"
#include "guiterm.h"
#include "berry_surface.h"
#include "filebrowser.h"
#include "sysmon.h"
#include "canvas_surface.h"
#include "settings_surface.h"
#include "cursor.h"

#define VERDANT_EXIT_NONE      0
#define VERDANT_EXIT_ESC       1
#define VERDANT_EXIT_CLOSED    2
#define VERDANT_EXIT_TIMEOUT   3
#define VERDANT_EXIT_ERROR     4

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
} verdant_report_t;


void verdant_init(void);
int  verdant_enter(void);
int  verdant_run(int max_seconds);
void verdant_leave(void);

int  verdant_is_running(void);
void verdant_request_exit(int reason);
int  verdant_get_exit_reason(void);
void verdant_get_report(verdant_report_t* out);

surface_t* verdant_create_surface(const char* title, const char* tag, int x, int y, int w, int h, uint32_t accent);
int        verdant_destroy_surface(surface_id_t id);
surface_t* verdant_get_surface(surface_id_t id);
surface_t* verdant_surface_at_index(int index);
int        verdant_surface_count(void);
surface_t* verdant_focused_surface(void);
int        verdant_focus_surface(surface_id_t id);
int        verdant_raise_surface(surface_id_t id);
int        verdant_set_bounds(surface_id_t id, int x, int y, int w, int h);
void       verdant_arrange_spatial(void);

int verdant_open_terminal(void);
int verdant_open_terminal_run(const char* filepath);
int verdant_open_files(void);
int verdant_open_sysmon(void);
int verdant_open_berry(void);
int verdant_open_canvas(void);
int verdant_open_settings(void);

#endif /* VERDANT_H */
