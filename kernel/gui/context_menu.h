/* ==============================================================================
 * Greenhouse OS — Modern Desktop Context Menu (Header)
 * ==============================================================================
 * Lightweight frosted-glass right-click context menu offering quick system
 * actions and window management shortcuts.
 * ==============================================================================
 */

#ifndef GUI_CONTEXT_MENU_H
#define GUI_CONTEXT_MENU_H

#include <stdint.h>
#include "../input/input.h"

#define CTX_ACTION_NONE     0
#define CTX_ACTION_TERMINAL 1
#define CTX_ACTION_FILES    2
#define CTX_ACTION_SYSMON   3
#define CTX_ACTION_BERRY    4
#define CTX_ACTION_TILE     5
#define CTX_ACTION_DESKTOP  6
#define CTX_ACTION_SETTINGS 7
#define CTX_ACTION_LOGOUT   8


void context_menu_init(void);
void context_menu_show(int x, int y);
void context_menu_hide(void);
int  context_menu_is_visible(void);
void context_menu_draw(void);
int  context_menu_handle_event(const input_event_t* ev, int* out_action);

#endif /* GUI_CONTEXT_MENU_H */
