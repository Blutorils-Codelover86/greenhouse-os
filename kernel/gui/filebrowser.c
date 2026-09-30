/* ==============================================================================
 * Greenhouse OS - Light-Mode File Browser (implementation)
 * ==============================================================================
 */

#include "filebrowser.h"
#include "compositor.h"
#include "verdant.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../vfs.h"

static void fb_strcpy(char* dst, int cap, const char* src) {
    int i = 0;
    if (src) {
        for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void fb_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

void filebrowser_load_dir(filebrowser_state_t* fb, const char* path) {
    if (!fb) return;
    fb_strcpy(fb->current_path, FB_PATH_MAX, path ? path : "C:\\");
    fb->current_drive = fb->current_path[0];
    fb->item_count = 0;
    fb->selected_index = -1;
    fb->has_preview = 0;

    vfs_node_t* node = vfs_resolve_path(NULL, fb->current_path);
    if (!node || !(node->flags & VFS_DIRECTORY)) return;

    vfs_dirent_t ent;
    for (uint32_t idx = 0; idx < FB_MAX_ENTRIES; idx++) {
        if (vfs_readdir(node, idx, &ent) != 0) break;
        fb_item_t* item = &fb->items[fb->item_count++];
        fb_strcpy(item->name, sizeof(item->name), ent.name);
        item->size = ent.size;
        item->is_dir = ent.is_dir;
    }
}

static void fb_preview_file(filebrowser_state_t* fb, const char* filename) {
    if (!fb || !filename) return;
    fb_strcpy(fb->preview_name, sizeof(fb->preview_name), filename);
    fb->has_preview = 1;
    fb->preview_len = 0;
    fb->preview_buf[0] = '\0';

    char full_path[FB_PATH_MAX];
    fb_strcpy(full_path, sizeof(full_path), fb->current_path);
    int len = 0;
    while (full_path[len]) len++;
    if (len > 0 && full_path[len - 1] != '\\' && full_path[len - 1] != '/') {
        full_path[len++] = '\\';
        full_path[len] = '\0';
    }
    fb_strcpy(full_path + len, sizeof(full_path) - len, filename);

    vfs_node_t* node = vfs_resolve_path(NULL, full_path);
    if (!node || (node->flags & VFS_DIRECTORY)) {
        fb_strcpy(fb->preview_buf, sizeof(fb->preview_buf), "<Directory>");
        fb->preview_len = 11;
        return;
    }

    int read_bytes = vfs_read(node, 0, sizeof(fb->preview_buf) - 1, (uint8_t*)fb->preview_buf);
    if (read_bytes > 0) {
        fb->preview_len = read_bytes;
        fb->preview_buf[read_bytes] = '\0';
    } else {
        fb_strcpy(fb->preview_buf, sizeof(fb->preview_buf), "<Empty File or Binary>");
        fb->preview_len = 22;
    }
}

void filebrowser_init(filebrowser_state_t* fb) {
    if (!fb) return;
    fb->current_drive = 'C';
    fb->scroll_offset = 0;
    fb->selected_index = -1;
    fb->has_preview = 0;
    filebrowser_load_dir(fb, "C:\\");
}

void filebrowser_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    filebrowser_state_t* fb = (filebrowser_state_t*)s->user_data;
    if (!fb) return;

    /* 1. Drive Selector Row */
    int d_y = cy + 8;
    int is_c = (fb->current_drive == 'C' || fb->current_drive == 'c');
    int is_r = (fb->current_drive == 'R' || fb->current_drive == 'r');

    /* C: Drive Button */
    graphics_fill_rounded_rect(cx + 8, d_y, 110, 22, 4, is_c ? GH_COLOR_GREEN_LEAF : GH_COLOR_SURFACE_HOVER);
    graphics_draw_rounded_rect(cx + 8, d_y, 110, 22, 4, GH_COLOR_BORDER_LIGHT);
    draw_text(cx + 14, d_y + 4, "[*] C:\\ (FAT32)", is_c ? GH_COLOR_WHITE : GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

    /* R: Drive Button */
    graphics_fill_rounded_rect(cx + 124, d_y, 110, 22, 4, is_r ? GH_COLOR_BLUE_INFO : GH_COLOR_SURFACE_HOVER);
    graphics_draw_rounded_rect(cx + 124, d_y, 110, 22, 4, GH_COLOR_BORDER_LIGHT);
    draw_text(cx + 130, d_y + 4, "[*] R:\\ (RAMFS)", is_r ? GH_COLOR_WHITE : GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

    /* Path readout */
    draw_text(cx + 242, d_y + 4, "Path: ", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text_clipped(cx + 284, d_y + 4, cw - 290, fb->current_path, GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);

    /* 2. File List Pane (Left half) */
    int list_w = (cw > 380) ? (cw / 2) - 4 : cw - 16;
    int pane_y = cy + 36;
    int pane_h = ch - 44;
    graphics_fill_rect(cx + 8, pane_y, list_w, pane_h, GH_COLOR_SURFACE);
    graphics_draw_rect(cx + 8, pane_y, list_w, pane_h, GH_COLOR_BORDER_LIGHT);

    int item_h = 20;
    int max_visible = pane_h / item_h;
    for (int i = 0; i < max_visible && (i + fb->scroll_offset) < fb->item_count; i++) {
        int idx = i + fb->scroll_offset;
        fb_item_t* it = &fb->items[idx];
        int iy = pane_y + i * item_h;
        int is_sel = (idx == fb->selected_index);

        if (is_sel) {
            graphics_fill_rect(cx + 9, iy, list_w - 2, item_h, GH_COLOR_SURFACE_HOVER);
        }

        /* Icon Tag */
        const char* tag = it->is_dir ? "[DIR]" : "[FILE]";
        uint32_t tag_col = it->is_dir ? GH_COLOR_AMBER_WARN : GH_COLOR_BLUE_INFO;
        draw_text(cx + 12, iy + 3, tag, tag_col, FONT_TRANSPARENT, 0);

        /* Filename */
        draw_text_clipped(cx + 64, iy + 3, list_w - 140, it->name, is_sel ? GH_COLOR_TEXT_PRIMARY : GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);

        /* Size */
        if (!it->is_dir) {
            char s_buf[20];
            int sn = 0;
            s_buf[0] = '\0';
            fb_put_uint(s_buf, &sn, sizeof(s_buf), it->size);
            s_buf[sn++] = 'B';
            s_buf[sn] = '\0';
            draw_text(cx + list_w - 60, iy + 3, s_buf, GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        }
    }

    /* 3. Preview Pane (Right half) */
    if (cw > 380) {
        int prev_x = cx + list_w + 14;
        int prev_w = cw - list_w - 22;
        graphics_fill_rect(prev_x, pane_y, prev_w, pane_h, GH_COLOR_SURFACE);
        graphics_draw_rect(prev_x, pane_y, prev_w, pane_h, GH_COLOR_BORDER_LIGHT);

        /* Preview Header */
        graphics_fill_rect(prev_x + 1, pane_y + 1, prev_w - 2, 22, GH_COLOR_SURFACE_HOVER);
        draw_text(prev_x + 8, pane_y + 4, "Preview: ", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        draw_text_clipped(prev_x + 68, pane_y + 4, prev_w - 74,
                          fb->has_preview ? fb->preview_name : "(Select a file)",
                          GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);

        /* Check if selected file is an ELF binary */
        int is_elf = 0;
        if (fb->has_preview) {
            int plen = 0;
            while (fb->preview_name[plen]) plen++;
            if (plen >= 4) {
                const char* ext = fb->preview_name + plen - 4;
                if (ext[0] == '.' && (ext[1] == 'e' || ext[1] == 'E') &&
                    (ext[2] == 'l' || ext[2] == 'L') && (ext[3] == 'f' || ext[3] == 'F')) {
                    is_elf = 1;
                }
            }
        }

        /* Preview Text Content */
        if (fb->has_preview) {
            int line_y = pane_y + 30;
            int gh = font_glyph_height();
            char line_buf[64];
            int li = 0;
            int r_lines = 0;
            int max_text_lines = is_elf ? 7 : 10;
            for (int p = 0; p < fb->preview_len && r_lines < max_text_lines; p++) {
                char c = fb->preview_buf[p];
                if (c == '\n' || li >= 48) {
                    line_buf[li] = '\0';
                    draw_text_clipped(prev_x + 8, line_y + r_lines * gh, prev_w - 16, line_buf, GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);
                    r_lines++;
                    li = 0;
                } else if (c >= 0x20 && c <= 0x7E) {
                    line_buf[li++] = c;
                }
            }
            if (li > 0 && r_lines < max_text_lines) {
                line_buf[li] = '\0';
                draw_text_clipped(prev_x + 8, line_y + r_lines * gh, prev_w - 16, line_buf, GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);
            }

            /* Draw [>] Run in Terminal action button for ELF executables */
            if (is_elf) {
                int btn_w = 144;
                int btn_h = 24;
                int btn_rx = prev_x + 8;
                int btn_ry = pane_y + pane_h - btn_h - 6;
                graphics_fill_rounded_rect(btn_rx, btn_ry, btn_w, btn_h, 4, GH_COLOR_GREEN_LEAF_SOFT);
                graphics_draw_rounded_rect(btn_rx, btn_ry, btn_w, btn_h, 4, GH_COLOR_GREEN_LEAF);
                draw_text(btn_rx + 8, btn_ry + 5, "[>] Run in Terminal", GH_COLOR_WHITE, FONT_TRANSPARENT, 0);
            }
        }
    }
}

int filebrowser_event(surface_t* s, const input_event_t* ev) {
    filebrowser_state_t* fb = (filebrowser_state_t*)s->user_data;
    if (!fb || !ev) return 0;

    int cx, cy, cw, ch;
    compositor_client_rect(s, &cx, &cy, &cw, &ch);

    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        int d_y = cy + 8;
        /* Drive button C: */
        if (ev->x >= cx + 8 && ev->x < cx + 118 && ev->y >= d_y && ev->y < d_y + 22) {
            filebrowser_load_dir(fb, "C:\\");
            return 1;
        }
        /* Drive button R: */
        if (ev->x >= cx + 124 && ev->x < cx + 234 && ev->y >= d_y && ev->y < d_y + 22) {
            filebrowser_load_dir(fb, "R:\\");
            return 1;
        }

        /* Check [>] Run in Terminal button click */
        if (cw > 380 && fb->has_preview) {
            int list_w = (cw / 2) - 4;
            int pane_y = cy + 36;
            int pane_h = ch - 44;
            int prev_x = cx + list_w + 14;
            int btn_w = 144;
            int btn_h = 24;
            int btn_rx = prev_x + 8;
            int btn_ry = pane_y + pane_h - btn_h - 6;

            int plen = 0;
            while (fb->preview_name[plen]) plen++;
            if (plen >= 4) {
                const char* ext = fb->preview_name + plen - 4;
                if (ext[0] == '.' && (ext[1] == 'e' || ext[1] == 'E') &&
                    (ext[2] == 'l' || ext[2] == 'L') && (ext[3] == 'f' || ext[3] == 'F')) {
                    if (ev->x >= btn_rx && ev->x <= btn_rx + btn_w &&
                        ev->y >= btn_ry && ev->y <= btn_ry + btn_h) {
                        /* Build full path and run in terminal */
                        char full_path[FB_PATH_MAX];
                        fb_strcpy(full_path, sizeof(full_path), fb->current_path);
                        int len = 0;
                        while (full_path[len]) len++;
                        if (len > 0 && full_path[len - 1] != '\\' && full_path[len - 1] != '/') {
                            full_path[len++] = '\\';
                            full_path[len] = '\0';
                        }
                        fb_strcpy(full_path + len, sizeof(full_path) - len, fb->preview_name);
                        verdant_open_terminal_run(full_path);
                        return 1;
                    }
                }
            }
        }

        /* File list clicks */
        int list_w = (cw > 380) ? (cw / 2) - 4 : cw - 16;
        int pane_y = cy + 36;
        int pane_h = ch - 44;
        if (ev->x >= cx + 8 && ev->x < cx + 8 + list_w && ev->y >= pane_y && ev->y < pane_y + pane_h) {
            int item_h = 20;
            int clicked_row = (ev->y - pane_y) / item_h;
            int idx = clicked_row + fb->scroll_offset;
            if (idx >= 0 && idx < fb->item_count) {
                fb->selected_index = idx;
                fb_item_t* it = &fb->items[idx];
                if (it->is_dir) {
                    /* Navigate into directory */
                    if (it->name[0] == '.' && it->name[1] == '.' && it->name[2] == '\0') {
                        /* Go to parent */
                        char parent[FB_PATH_MAX];
                        fb_strcpy(parent, sizeof(parent), fb->current_path);
                        int len = 0;
                        while (parent[len]) len++;
                        if (len > 3) {
                            if (parent[len - 1] == '\\' || parent[len - 1] != '/') len--;
                            while (len > 3 && parent[len - 1] != '\\' && parent[len - 1] != '/') len--;
                            parent[len] = '\0';
                        }
                        filebrowser_load_dir(fb, parent);
                    } else if (it->name[0] != '.') {
                        char sub[FB_PATH_MAX];
                        fb_strcpy(sub, sizeof(sub), fb->current_path);
                        int len = 0;
                        while (sub[len]) len++;
                        if (len > 0 && sub[len - 1] != '\\' && sub[len - 1] != '/') {
                            sub[len++] = '\\';
                            sub[len] = '\0';
                        }
                        fb_strcpy(sub + len, sizeof(sub) - len, it->name);
                        filebrowser_load_dir(fb, sub);
                    }
                } else {
                    /* Preview file */
                    fb_preview_file(fb, it->name);
                }
                return 1;
            }
        }
    }

    if (ev->type == INPUT_EVENT_MOUSE_WHEEL) {
        if (ev->dy < 0 && fb->scroll_offset > 0) fb->scroll_offset--;
        if (ev->dy > 0 && fb->scroll_offset < fb->item_count - 1) fb->scroll_offset++;
        return 1;
    }

    return 0;
}