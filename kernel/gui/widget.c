/* ==============================================================================
 * Greenhouse OS - Phase 7: Widgets (implementation)
 * ==============================================================================
 */

#include "widget.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

static int widget_text_len(const char* s) {
    int n = 0;
    if (s) while (s[n]) n++;
    return n;
}

static void widget_text(const widget_t* w, const char* s, uint32_t color) {
    if (!s) return;
    int tw = widget_text_len(s) * (font_glyph_width() + 1);
    int tx = w->x + (w->w - tw) / 2;
    int ty = w->y + (w->h - font_glyph_height()) / 2;
    if (tx < w->x) tx = w->x;
    draw_text(tx, ty, s, color, w->bg, 1);
}

void widget_init_array(widget_t* widgets, int count) {
    if (!widgets) return;
    for (int i = 0; i < count; i++) {
        widgets[i].type = WIDGET_NONE;
        widgets[i].x = widgets[i].y = 0;
        widgets[i].w = widgets[i].h = 0;
        widgets[i].text[0] = '\0';
        widgets[i].enabled = 1;
        widgets[i].pressed = 0;
        widgets[i].checked = 0;
        widgets[i].percent = 0;
        widgets[i].bg = 0xFF2D3748;
        widgets[i].fg = 0xFFE8EDF5;
        widgets[i].border = 0xFF4A5568;
        widgets[i].accent = 0xFF4C8DFF;
        widgets[i].user_data = 0;
    }
}

void widget_set_rect(widget_t* w, int x, int y, int width, int height) {
    if (!w) return;
    w->x = x; w->y = y; w->w = width; w->h = height;
}

void widget_set_text(widget_t* w, const char* text) {
    if (!w) return;
    int i = 0;
    if (text) {
        for (; text[i] && i < (int)sizeof(w->text) - 1; i++) w->text[i] = text[i];
    }
    w->text[i] = '\0';
}

void widget_set_pressed(widget_t* w, int pressed) {
    if (w) w->pressed = pressed ? 1 : 0;
}

void widget_set_checked(widget_t* w, int checked) {
    if (w) w->checked = checked ? 1 : 0;
}

void widget_set_percent(widget_t* w, int percent) {
    if (!w) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    w->percent = percent;
}

int widget_is_checked(const widget_t* w)    { return w ? w->checked : 0; }
int widget_get_percent(const widget_t* w)  { return w ? w->percent : 0; }

void widget_draw(const widget_t* w) {
    if (!w || !w->enabled || w->type == WIDGET_NONE) return;

    switch (w->type) {
        case WIDGET_LABEL:
            draw_text(w->x, w->y + (w->h - font_glyph_height()) / 2, w->text, w->fg, w->bg, 1);
            break;

        case WIDGET_BUTTON:
            graphics_fill_rect(w->x, w->y, w->w, w->h, w->pressed ? w->border : w->bg);
            graphics_draw_rect(w->x, w->y, w->w, w->h, w->accent);
            widget_text(w, w->text, w->fg);
            break;

        case WIDGET_CHECKBOX: {
            int box = 16;
            int by = w->y + (w->h - box) / 2;
            graphics_fill_rect(w->x, by, box, box, w->bg);
            graphics_draw_rect(w->x, by, box, box, w->border);
            if (w->checked) {
                graphics_draw_line(w->x + 3, by + 8, w->x + 7, by + 12, w->accent);
                graphics_draw_line(w->x + 7, by + 12, w->x + 13, by + 4, w->accent);
            }
            draw_text(w->x + box + 8, w->y + (w->h - font_glyph_height()) / 2,
                      w->text, w->fg, w->bg, 1);
            break;
        }

        case WIDGET_PROGRESS: {
            graphics_fill_rect(w->x, w->y, w->w, w->h, w->bg);
            graphics_draw_rect(w->x, w->y, w->w, w->h, w->border);
            int inner = w->w - 4;
            if (inner > 0) graphics_fill_rect(w->x + 2, w->y + 2, (inner * w->percent) / 100, w->h - 4, w->accent);
            char label[8];
            label[0] = '%'; label[1] = (char)('0' + (w->percent / 10) % 10);
            label[2] = (char)('0' + w->percent % 10);
            label[3] = '\0';
            int tw = widget_text_len(label) * (font_glyph_width() + 1);
            draw_text(w->x + (w->w - tw) / 2, w->y + (w->h - font_glyph_height()) / 2,
                      label, 0xFF101828, w->bg, 1);
            break;
        }

        default:
            break;
    }
}

int widget_handle_mouse(widget_t* w, int px, int py, int pressed) {
    if (!w || !w->enabled || w->type == WIDGET_NONE) return 0;
    if (px < w->x || py < w->y || px >= w->x + w->w || py >= w->y + w->h) {
        if (w->type == WIDGET_BUTTON) w->pressed = 0;
        return 0;
    }

    if (w->type == WIDGET_BUTTON) {
        w->pressed = pressed;
        return pressed;
    }
    if (w->type == WIDGET_CHECKBOX) {
        if (pressed) w->checked = !w->checked;
        return pressed;
    }
    return 0;
}

void widget_draw_all(const widget_t* widgets, int count) {
    for (int i = 0; i < count; i++) widget_draw(&widgets[i]);
}

int widget_handle_mouse_all(widget_t* widgets, int count, int px, int py, int pressed) {
    for (int i = 0; i < count; i++) {
        if (widget_handle_mouse(&widgets[i], px, py, pressed)) return 1;
    }
    return 0;
}
