/* ==============================================================================
 * Greenhouse OS - Light-Mode System Monitor (implementation)
 * ==============================================================================
 */

#include "sysmon.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "gh_theme.h"
#include "../pmm.h"
#include "../heap.h"
#include "../process.h"
#include "../irq.h"
#include "../vfs.h"
#include "../cpu.h"

static void sm_strcpy(char* dst, int cap, const char* src) {
    int i = 0;
    if (src) {
        for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void sm_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

void sysmon_init(void) {
}

void sysmon_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    (void)s; (void)ch;

    pmm_stats_t pmm = pmm_get_stats();
    heap_stats_t hp = heap_get_stats();

    /* 1. Header Banner */
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 26, GH_COLOR_GREEN_LEAF_SOFT, GH_COLOR_GREEN_LEAF_DEEP);
    graphics_draw_rounded_rect(cx + 8, cy + 8, cw - 16, 26, 4, GH_COLOR_GREEN_LEAF);
    draw_text(cx + 14, cy + 13, "[*] SYSTEM TELEMETRY & RESOURCES", GH_COLOR_WHITE, FONT_TRANSPARENT, 1);

    /* CPU Hardware Info */
    int y = cy + 40;
    const CPUInfo* cpu = get_cpu_info();
    if (cpu && cpu->brand[0]) {
        draw_text(cx + 12, y, "CPU: ", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        draw_text_clipped(cx + 46, y, cw - 60, cpu->brand, GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);
    } else {
        draw_text(cx + 12, y, "CPU: N/A", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    }
    y += 18;

    /* 2. PMM Physical Memory Bar */
    uint64_t total_mb = pmm.usable_memory_bytes / (1024 * 1024);
    uint64_t free_mb = (pmm.free_frames * 4096) / (1024 * 1024);
    uint64_t used_mb = (total_mb >= free_mb) ? (total_mb - free_mb) : 0;
    int pmm_pct = (total_mb > 0) ? (int)((used_mb * 100) / total_mb) : 0;

    char pmm_str[64];
    int pn = 0;
    sm_strcpy(pmm_str, sizeof(pmm_str), "Physical RAM: ");
    pn = 14;
    sm_put_uint(pmm_str, &pn, sizeof(pmm_str), used_mb);
    sm_strcpy(pmm_str + pn, sizeof(pmm_str) - pn, " / ");
    pn += 3;
    sm_put_uint(pmm_str, &pn, sizeof(pmm_str), total_mb);
    sm_strcpy(pmm_str + pn, sizeof(pmm_str) - pn, " MB (");
    pn += 5;
    sm_put_uint(pmm_str, &pn, sizeof(pmm_str), (uint64_t)pmm_pct);
    sm_strcpy(pmm_str + pn, sizeof(pmm_str) - pn, "%)");
    draw_text(cx + 12, y, pmm_str, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

    /* Progress bar */
    y += 18;
    int bar_w = cw - 24;
    graphics_fill_rect(cx + 12, y, bar_w, 12, GH_COLOR_SURFACE_HOVER);
    graphics_draw_rect(cx + 12, y, bar_w, 12, GH_COLOR_BORDER_LIGHT);
    int fill_w = (bar_w - 2) * pmm_pct / 100;
    if (fill_w > 0) {
        graphics_fill_gradient_h(cx + 13, y + 1, fill_w, 10, GH_COLOR_GREEN_LEAF_DEEP, GH_COLOR_GREEN_LEAF);
    }

    /* 3. Kernel Heap Bar */
    y += 20;
    uint64_t h_total_kb = hp.total_bytes / 1024;
    uint64_t h_used_kb = hp.used_bytes / 1024;
    int hp_pct = (h_total_kb > 0) ? (int)((h_used_kb * 100) / h_total_kb) : 0;

    char hp_str[64];
    int hn = 0;
    sm_strcpy(hp_str, sizeof(hp_str), "Kernel Heap:  ");
    hn = 14;
    sm_put_uint(hp_str, &hn, sizeof(hp_str), h_used_kb);
    sm_strcpy(hp_str + hn, sizeof(hp_str) - hn, " / ");
    hn += 3;
    sm_put_uint(hp_str, &hn, sizeof(hp_str), h_total_kb);
    sm_strcpy(hp_str + hn, sizeof(hp_str) - hn, " KB (Allocs: ");
    hn += 13;
    sm_put_uint(hp_str, &hn, sizeof(hp_str), (uint64_t)hp.active_allocs);
    sm_strcpy(hp_str + hn, sizeof(hp_str) - hn, ")");
    draw_text(cx + 12, y, hp_str, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

    y += 18;
    graphics_fill_rect(cx + 12, y, bar_w, 12, GH_COLOR_SURFACE_HOVER);
    graphics_draw_rect(cx + 12, y, bar_w, 12, GH_COLOR_BORDER_LIGHT);
    int h_fill_w = (bar_w - 2) * hp_pct / 100;
    if (h_fill_w > 0) {
        graphics_fill_gradient_h(cx + 13, y + 1, h_fill_w, 10, GH_COLOR_BLUE_INFO, GH_COLOR_GREEN_LEAF);
    }

    /* 4. Active Process List Table */
    y += 24;
    draw_text(cx + 12, y, "PROCESS TABLE:", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    y += 18;

    /* Table Header */
    graphics_fill_rect(cx + 12, y, bar_w, 18, GH_COLOR_SURFACE_HOVER);
    draw_text(cx + 16, y + 2, "PID", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(cx + 60, y + 2, "NAME", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(cx + 180, y + 2, "STATE", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(cx + 270, y + 2, "MODE", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    y += 20;

    int p_cnt = process_count();
    int gh = font_glyph_height();
    for (int i = 0; i < p_cnt && i < 8; i++) {
        process_t* proc = process_get_by_index((size_t)i);
        if (!proc) continue;

        char pid_str[12];
        int pd = 0;
        pid_str[0] = '\0';
        sm_put_uint(pid_str, &pd, sizeof(pid_str), (uint64_t)proc->pid);

        const char* st_str = "READY";
        if (proc->state == PROCESS_RUNNING) st_str = "RUNNING";
        else if (proc->state == PROCESS_SLEEPING) st_str = "SLEEP";
        else if (proc->state == PROCESS_BLOCKED) st_str = "BLOCKED";
        else if (proc->state == PROCESS_TERMINATED) st_str = "DEAD";

        const char* mode_str = proc->is_user ? "Ring 3 User" : "Ring 0 Kernel";
        uint32_t row_col = proc->is_user ? GH_COLOR_GREEN_LEAF_DEEP : GH_COLOR_BLUE_INFO;

        draw_text(cx + 16, y + i * gh, pid_str, GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        draw_text_clipped(cx + 60, y + i * gh, 110, proc->name, row_col, FONT_TRANSPARENT, 0);
        draw_text(cx + 180, y + i * gh, st_str, (proc->state == PROCESS_RUNNING) ? GH_COLOR_GREEN_LEAF_DEEP : GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        draw_text(cx + 270, y + i * gh, mode_str, GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    }
}

int sysmon_event(surface_t* s, const input_event_t* ev) {
    (void)s; (void)ev;
    return 0;
}