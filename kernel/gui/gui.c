/* ==============================================================================
 * Greenhouse OS - Phase 7: GUI / Verdant Integration Wrapper
 * ==============================================================================
 */

#include "gui.h"
#include "verdant.h"

void gui_init(void) {
    verdant_init();
}

int gui_enter(void) {
    return verdant_enter();
}

int gui_run(int max_seconds) {
    return verdant_run(max_seconds);
}

void gui_leave(void) {
    verdant_leave();
}

int  gui_is_running(void) { return verdant_is_running(); }
void gui_request_exit(int reason) { verdant_request_exit(reason); }
int  gui_get_exit_reason(void) { return verdant_get_exit_reason(); }
void gui_get_report(gui_report_t* out) {
    if (!out) return;
    verdant_report_t vr;
    verdant_get_report(&vr);
    out->width = vr.width;
    out->height = vr.height;
    out->bpp = vr.bpp;
    out->backend_multiboot = vr.backend_multiboot;
    out->back_buffer = vr.back_buffer;
    out->back_buffer_size = vr.back_buffer_size;
    out->presents = vr.presents;
    out->frames = vr.frames;
    out->events_processed = vr.events_processed;
    out->exit_reason = vr.exit_reason;
}

int gui_open_terminal(void) {
    return verdant_open_terminal();
}

int gui_open_about(void) {
    return verdant_open_settings();
}
