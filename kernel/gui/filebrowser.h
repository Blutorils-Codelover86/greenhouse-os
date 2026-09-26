/* ==============================================================================
 * Greenhouse OS - VERDANT File Browser
 * ==============================================================================
 */

#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <stdint.h>
#include "surface.h"
#include "../vfs.h"

#define FB_MAX_ENTRIES 32
#define FB_PATH_MAX    128

typedef struct {
    char     name[64];
    uint32_t size;
    uint8_t  is_dir;
} fb_item_t;

typedef struct {
    char      current_path[FB_PATH_MAX];
    char      current_drive; /* 'C' or 'R' */
    fb_item_t items[FB_MAX_ENTRIES];
    int       item_count;
    int       selected_index;
    int       scroll_offset;

    /* Preview buffer */
    char      preview_name[64];
    char      preview_buf[512];
    int       preview_len;
    int       has_preview;
} filebrowser_state_t;

void filebrowser_init(filebrowser_state_t* fb);
void filebrowser_load_dir(filebrowser_state_t* fb, const char* path);
void filebrowser_draw(surface_t* s, int cx, int cy, int cw, int ch);
int  filebrowser_event(surface_t* s, const input_event_t* ev);

#endif /* FILEBROWSER_H */
