/* ==============================================================================
 * Greenhouse OS — Dedicated Renderer Visual Benchmark Screen
 * ==============================================================================
 * Phase 3 Visual Benchmark Screen implementing:
 *   - Light-mode botanical background with subtle ambient radial blooms
 *   - Physical frosted glass panels with specular highlights & soft drop shadows
 *   - Linear gradients (vertical, horizontal, 45-degree botanical)
 *   - Radial gradients (ambient halo bloom)
 *   - Antialiased rounded rectangles with subpixel coverage
 *   - Complete component system (buttons, badges, inputs, meters)
 *   - Interactive Greenhouse cursor family showcase (all 7 shapes)
 * ==============================================================================
 */

#include "test_screen.h"
#include "renderer.h"
#include "cursor.h"
#include "gh_theme.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../input/input.h"
#include "../irq.h"

static int g_test_running = 0;

static void draw_centered_text(int x, int y, int w, int h, const char* str, uint32_t color) {
    if (!str || !str[0]) return;
    int tw = font_text_width(str);
    int th = font_glyph_height();
    int tx = x + (w - tw) / 2;
    int ty = y + (h - th) / 2;
    draw_text(tx, ty, str, color, FONT_TRANSPARENT, 0);
}

int test_screen_run(int max_seconds) {
    if (graphics_enter() != 0) {
        return -1;
    }

    cursor_init();
    cursor_set_shape(CURSOR_DEFAULT);
    cursor_set_visible(1);

    int sw = graphics_get_width();
    int sh = graphics_get_height();

    int mx = sw / 2;
    int my = sh / 2;
    input_set_pointer(mx, my);

    g_test_running = 1;
    uint64_t start_ticks = timer_get_ticks();
    uint32_t freq = timer_get_frequency();
    uint64_t max_ticks = (max_seconds > 0) ? ((uint64_t)max_seconds * freq) : 0;

    int active_cursor_shape = CURSOR_DEFAULT;
    int focused_input = 1;
    (void)focused_input;

    while (g_test_running) {
        /* 1. Process Input Events */
        input_event_t ev;
        while (input_poll_event(&ev)) {
            if (ev.type == INPUT_EVENT_KEY_DOWN) {
                if (ev.scancode == KBD_SCAN_ESC || ev.ascii == 27 || ev.ascii == 'q' || ev.ascii == 'Q') {
                    g_test_running = 0;
                    break;
                }
                if (ev.ascii >= '1' && ev.ascii <= '7') {
                    active_cursor_shape = ev.ascii - '1';
                    cursor_set_shape(active_cursor_shape);
                }
                if (ev.ascii == '+' || ev.ascii == '=') {
                    cursor_set_scale(2);
                }
                if (ev.ascii == '-' || ev.ascii == '_') {
                    cursor_set_scale(1);
                }
            } else if (ev.type == INPUT_EVENT_MOUSE_MOVE ||
                       ev.type == INPUT_EVENT_MOUSE_BUTTON_DOWN ||
                       ev.type == INPUT_EVENT_MOUSE_BUTTON_UP) {
                input_get_pointer(&mx, &my);

                /* Check if hovering over cursor showcase boxes (bottom card) */
                int box_y = 574;
                int box_h = 96;
                int box_w = 126;
                int gap = 12;
                int start_x = 24 + 14;

                if (my >= box_y && my < box_y + box_h) {
                    for (int i = 0; i < CURSOR_TYPE_COUNT; i++) {
                        int bx = start_x + i * (box_w + gap);
                        if (mx >= bx && mx < bx + box_w) {
                            active_cursor_shape = i;
                            cursor_set_shape(i);
                            break;
                        }
                    }
                }
            }
        }

        if (!g_test_running) break;

        /* Check timeout */
        if (max_ticks > 0 && (timer_get_ticks() - start_ticks) >= max_ticks) {
            g_test_running = 0;
            break;
        }

        /* 2. Begin Frame */
        graphics_begin_frame();

        /* 3. Calm Botanical Light Background with Ambient Bloom */
        renderer_draw_light_background(sw, sh);

        /* 4. Top Header Floating Glass Panel */
        int hdr_x = 24, hdr_y = 16, hdr_w = sw - 48, hdr_h = 46;
        renderer_draw_glass_panel(hdr_x, hdr_y, hdr_w, hdr_h, 10, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 2);

        /* Logo badge */
        renderer_fill_aa_rounded_rect(hdr_x + 12, hdr_y + 10, 26, 26, 6, GH_COLOR_GREEN_FOREST, 255);
        draw_text(hdr_x + 16, hdr_y + 15, "GH", GH_COLOR_WHITE, FONT_TRANSPARENT, 0);

        /* Title text */
        draw_text(hdr_x + 46, hdr_y + 14, "GREENHOUSE OS  --  RENDERING FOUNDATION VISUAL BENCHMARK",
                  GH_COLOR_GREEN_FOREST, FONT_TRANSPARENT, 0);

        /* Badges on right side */
        int badge_rx = hdr_x + hdr_w - 14;
        badge_rx -= (font_text_width("60 FPS") + 16);
        renderer_draw_badge(badge_rx, hdr_y + 12, "60 FPS", GH_COLOR_GREEN_PILL_BG, GH_COLOR_GREEN_PILL_FG, 4);
        badge_rx -= (font_text_width("SUBPIXEL AA") + 20);
        renderer_draw_badge(badge_rx, hdr_y + 12, "SUBPIXEL AA", GH_COLOR_SURFACE_HOVER, GH_COLOR_TEXT_SECONDARY, 4);
        badge_rx -= (font_text_width("PHYSICAL GLASS") + 20);
        renderer_draw_badge(badge_rx, hdr_y + 12, "PHYSICAL GLASS", GH_COLOR_SURFACE_HOVER, GH_COLOR_TEXT_SECONDARY, 4);
        badge_rx -= (font_text_width("LIGHT MODE") + 20);
        renderer_draw_badge(badge_rx, hdr_y + 12, "LIGHT MODE", GH_COLOR_GREEN_PILL_BG, GH_COLOR_GREEN_PILL_FG, 4);

        /* ----------------------------------------------------------------------
         * Card 1: Top-Left (Physical Frosted Glass & Translucency)
         * -------------------------------------------------------------------- */
        int c1_x = 24, c1_y = 72, c1_w = (sw - 72) / 2, c1_h = 216;
        renderer_draw_glass_panel(c1_x, c1_y, c1_w, c1_h, 12, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 3);

        /* Header dot & title */
        renderer_fill_aa_circle(c1_x + 18, c1_y + 18, 4, GH_COLOR_GREEN_LEAF, 255);
        draw_text(c1_x + 28, c1_y + 11, "PHYSICAL FROSTED GLASS & TRANSLUCENCY", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        renderer_fill_alpha_rect(c1_x + 12, c1_y + 32, c1_w - 24, 1, GH_COLOR_BORDER_LIGHT, 140);

        /* Subcard 1A: Translucent white glass */
        int sc1_w = (c1_w - 36) / 2;
        int sc1_h = 110;
        int sc1_x = c1_x + 14;
        int sc1_y = c1_y + 44;
        renderer_draw_glass_panel(sc1_x, sc1_y, sc1_w, sc1_h, 8, GH_COLOR_SURFACE, 220, GH_COLOR_BORDER_LIGHT, 2);
        draw_text(sc1_x + 12, sc1_y + 10, "Base Surface (94%)", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        draw_text(sc1_x + 12, sc1_y + 32, "Soft ambient shadow", GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);
        draw_text(sc1_x + 12, sc1_y + 50, "1px specular top rim", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        renderer_draw_badge(sc1_x + 12, sc1_y + 76, "ALPHA: 240/255", GH_COLOR_GREEN_PILL_BG, GH_COLOR_GREEN_PILL_FG, 3);

        /* Subcard 1B: Elevated physical card */
        int sc2_x = sc1_x + sc1_w + 10;
        renderer_draw_glass_panel(sc2_x, sc1_y, sc1_w, sc1_h, 8, GH_COLOR_SURFACE, 250, GH_COLOR_BORDER_LIGHT, 4);
        draw_text(sc2_x + 12, sc1_y + 10, "Elevated Layer (98%)", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        draw_text(sc2_x + 12, sc1_y + 32, "Quadratic decay falloff", GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);
        draw_text(sc2_x + 12, sc1_y + 50, "Zero stepping artifacts", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        renderer_draw_badge(sc2_x + 12, sc1_y + 76, "SHADOW: 4-TIER", GH_COLOR_SURFACE_HOVER, GH_COLOR_TEXT_SECONDARY, 3);

        /* Bottom transmittance meter */
        draw_text(c1_x + 14, c1_y + 166, "Physical Transmittance Gauge", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        renderer_draw_meter(c1_x + 14, c1_y + 184, c1_w - 28, 16, 92, GH_COLOR_GREEN_LEAF, GH_COLOR_SURFACE_HOVER, GH_COLOR_BORDER_LIGHT, 8);

        /* ----------------------------------------------------------------------
         * Card 2: Top-Right (Gradients & Ambient Radial Bloom)
         * -------------------------------------------------------------------- */
        int c2_x = c1_x + c1_w + 24, c2_y = 72, c2_w = c1_w, c2_h = 216;
        renderer_draw_glass_panel(c2_x, c2_y, c2_w, c2_h, 12, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 3);

        renderer_fill_aa_circle(c2_x + 18, c2_y + 18, 4, GH_COLOR_GREEN_LEAF, 255);
        draw_text(c2_x + 28, c2_y + 11, "GRADIENTS & AMBIENT RADIAL BLOOM", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        renderer_fill_alpha_rect(c2_x + 12, c2_y + 32, c2_w - 24, 1, GH_COLOR_BORDER_LIGHT, 140);

        int sw_w = (c2_w - 56) / 4;
        int sw_h = 105;
        int sw_y = c2_y + 46;

        /* Swatch 1: Vertical linear */
        int sw1_x = c2_x + 14;
        renderer_fill_gradient_v(sw1_x, sw_y, sw_w, sw_h, GH_COLOR_GREEN_FOREST, GH_COLOR_GREEN_LEAF, 255);
        renderer_draw_aa_rounded_rect(sw1_x, sw_y, sw_w, sw_h, 6, 1, GH_COLOR_BORDER_LIGHT, 180);
        draw_centered_text(sw1_x, sw_y + sw_h + 6, sw_w, 14, "Vertical", GH_COLOR_TEXT_SECONDARY);

        /* Swatch 2: Horizontal linear */
        int sw2_x = sw1_x + sw_w + 9;
        renderer_fill_gradient_h(sw2_x, sw_y, sw_w, sw_h, GH_COLOR_GREEN_SPROUT, GH_COLOR_GREEN_LEAF_DEEP, 255);
        renderer_draw_aa_rounded_rect(sw2_x, sw_y, sw_w, sw_h, 6, 1, GH_COLOR_BORDER_LIGHT, 180);
        draw_centered_text(sw2_x, sw_y + sw_h + 6, sw_w, 14, "Horizontal", GH_COLOR_TEXT_SECONDARY);

        /* Swatch 3: 45-degree botanical */
        int sw3_x = sw2_x + sw_w + 9;
        renderer_fill_gradient_linear(sw3_x, sw_y, sw_w, sw_h, GH_COLOR_GREEN_LEAF, GH_COLOR_GREEN_SPROUT, 45, 255);
        renderer_draw_aa_rounded_rect(sw3_x, sw_y, sw_w, sw_h, 6, 1, GH_COLOR_BORDER_LIGHT, 180);
        draw_centered_text(sw3_x, sw_y + sw_h + 6, sw_w, 14, "Botanical 45", GH_COLOR_TEXT_SECONDARY);

        /* Swatch 4: Ambient radial bloom */
        int sw4_x = sw3_x + sw_w + 9;
        renderer_fill_alpha_rect(sw4_x, sw_y, sw_w, sw_h, GH_COLOR_SURFACE_MUTED, 255);
        renderer_fill_radial_gradient(sw4_x + sw_w / 2, sw_y + sw_h / 2, sw_w / 2, GH_COLOR_GREEN_SPROUT, GH_COLOR_SURFACE_MUTED, 220);
        renderer_draw_aa_rounded_rect(sw4_x, sw_y, sw_w, sw_h, 6, 1, GH_COLOR_BORDER_LIGHT, 180);
        draw_centered_text(sw4_x, sw_y + sw_h + 6, sw_w, 14, "Radial Bloom", GH_COLOR_TEXT_SECONDARY);

        /* Note at bottom */
        draw_text(c2_x + 14, c2_y + 184, "32-bit interpolated color stops with integer Euclidean distance",
                  GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

        /* ----------------------------------------------------------------------
         * Card 3: Middle-Left (Antialiased Rounded Rectangles & Soft Shadows)
         * -------------------------------------------------------------------- */
        int c3_x = 24, c3_y = 300, c3_w = c1_w, c3_h = 216;
        renderer_draw_glass_panel(c3_x, c3_y, c3_w, c3_h, 12, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 3);

        renderer_fill_aa_circle(c3_x + 18, c3_y + 18, 4, GH_COLOR_GREEN_LEAF, 255);
        draw_text(c3_x + 28, c3_y + 11, "ANTIALIASED ROUNDED RECTANGLES & SHADOWS", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        renderer_fill_alpha_rect(c3_x + 12, c3_y + 32, c3_w - 24, 1, GH_COLOR_BORDER_LIGHT, 140);

        /* Antialiased shapes with varying radii */
        int rw_w = (c3_w - 56) / 4;
        int rw_h = 42;
        int rw_y = c3_y + 46;

        /* R = 4px */
        int rw1_x = c3_x + 14;
        renderer_fill_aa_rounded_rect(rw1_x, rw_y, rw_w, rw_h, 4, GH_COLOR_GREEN_FOREST, 255);
        draw_centered_text(rw1_x, rw_y + 14, rw_w, 14, "R = 4px", GH_COLOR_WHITE);

        /* R = 8px */
        int rw2_x = rw1_x + rw_w + 9;
        renderer_fill_aa_rounded_rect(rw2_x, rw_y, rw_w, rw_h, 8, GH_COLOR_GREEN_LEAF, 255);
        draw_centered_text(rw2_x, rw_y + 14, rw_w, 14, "R = 8px", GH_COLOR_WHITE);

        /* R = 14px */
        int rw3_x = rw2_x + rw_w + 9;
        renderer_fill_aa_rounded_rect(rw3_x, rw_y, rw_w, rw_h, 14, GH_COLOR_GREEN_LEAF_DEEP, 255);
        draw_centered_text(rw3_x, rw_y + 14, rw_w, 14, "R = 14px", GH_COLOR_WHITE);

        /* R = Pill */
        int rw4_x = rw3_x + rw_w + 9;
        renderer_fill_aa_rounded_rect(rw4_x, rw_y, rw_w, rw_h, rw_h / 2, GH_COLOR_GREEN_SPROUT, 255);
        draw_centered_text(rw4_x, rw_y + 14, rw_w, 14, "R = Pill", GH_COLOR_GREEN_FOREST);

        /* Subpixel antialiased outline demonstration */
        int ow_y = rw_y + rw_h + 16;
        renderer_draw_aa_rounded_rect(rw1_x, ow_y, rw_w, rw_h, 6, 1, GH_COLOR_GREEN_FOREST, 220);
        draw_centered_text(rw1_x, ow_y + 14, rw_w, 14, "1px Stroke", GH_COLOR_TEXT_PRIMARY);

        renderer_draw_aa_rounded_rect(rw2_x, ow_y, rw_w, rw_h, 10, 2, GH_COLOR_GREEN_LEAF, 220);
        draw_centered_text(rw2_x, ow_y + 14, rw_w, 14, "2px Stroke", GH_COLOR_TEXT_PRIMARY);

        /* Ambient soft drop shadow sample */
        int sh_box_x = rw3_x;
        int sh_box_w = rw_w * 2 + 9;
        renderer_draw_soft_shadow(sh_box_x, ow_y, sh_box_w, rw_h, 10, 8, 0, 3, 0x14221D, 45);
        renderer_fill_aa_rounded_rect(sh_box_x, ow_y, sh_box_w, rw_h, 10, GH_COLOR_SURFACE, 255);
        renderer_draw_aa_rounded_rect(sh_box_x, ow_y, sh_box_w, rw_h, 10, 1, GH_COLOR_BORDER_LIGHT, 200);
        draw_centered_text(sh_box_x, ow_y + 14, sh_box_w, 14, "Ambient Soft Drop Shadow", GH_COLOR_TEXT_PRIMARY);

        draw_text(c3_x + 14, c3_y + 184, "Subpixel corner antialiasing: fractional coverage eliminates stepping",
                  GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

        /* ----------------------------------------------------------------------
         * Card 4: Middle-Right (Component Design System & Buttons)
         * -------------------------------------------------------------------- */
        int c4_x = c2_x, c4_y = 300, c4_w = c2_w, c4_h = 216;
        renderer_draw_glass_panel(c4_x, c4_y, c4_w, c4_h, 12, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 3);

        renderer_fill_aa_circle(c4_x + 18, c4_y + 18, 4, GH_COLOR_GREEN_LEAF, 255);
        draw_text(c4_x + 28, c4_y + 11, "COMPONENT DESIGN SYSTEM & BUTTON STATES", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        renderer_fill_alpha_rect(c4_x + 12, c4_y + 32, c4_w - 24, 1, GH_COLOR_BORDER_LIGHT, 140);

        /* Row 1: Primary, Brand, Destructive buttons */
        int btn_w = (c4_w - 42) / 3;
        int btn_h = 32;
        int btn_y1 = c4_y + 44;

        int b1_x = c4_x + 14;
        int b1_hov = (mx >= b1_x && mx < b1_x + btn_w && my >= btn_y1 && my < btn_y1 + btn_h);
        renderer_draw_button_styled(b1_x, btn_y1, btn_w, btn_h, "Save Changes", RENDERER_BTN_PRIMARY, b1_hov, 0);

        int b2_x = b1_x + btn_w + 7;
        int b2_hov = (mx >= b2_x && mx < b2_x + btn_w && my >= btn_y1 && my < btn_y1 + btn_h);
        renderer_draw_button_styled(b2_x, btn_y1, btn_w, btn_h, "Deploy App", RENDERER_BTN_BRAND, b2_hov, 0);

        int b3_x = b2_x + btn_w + 7;
        int b3_hov = (mx >= b3_x && mx < b3_x + btn_w && my >= btn_y1 && my < btn_y1 + btn_h);
        renderer_draw_button_styled(b3_x, btn_y1, btn_w, btn_h, "Terminate", RENDERER_BTN_DESTRUCTIVE, b3_hov, 0);

        /* Row 2: Secondary, Outline, Ghost buttons */
        int btn_y2 = btn_y1 + btn_h + 8;
        int b4_hov = (mx >= b1_x && mx < b1_x + btn_w && my >= btn_y2 && my < btn_y2 + btn_h);
        renderer_draw_button_styled(b1_x, btn_y2, btn_w, btn_h, "Cancel", RENDERER_BTN_SECONDARY, b4_hov, 0);

        int b5_hov = (mx >= b2_x && mx < b2_x + btn_w && my >= btn_y2 && my < btn_y2 + btn_h);
        renderer_draw_button_styled(b2_x, btn_y2, btn_w, btn_h, "Inspect", RENDERER_BTN_OUTLINE, b5_hov, 0);

        int b6_hov = (mx >= b3_x && mx < b3_x + btn_w && my >= btn_y2 && my < btn_y2 + btn_h);
        renderer_draw_button_styled(b3_x, btn_y2, btn_w, btn_h, "Ghost", RENDERER_BTN_GHOST, b6_hov, 0);

        /* Row 3: Input Field & Badges */
        int inp_y = btn_y2 + btn_h + 10;
        int inp_w = (c4_w * 3) / 5;
        renderer_draw_input_field(b1_x, inp_y, inp_w, 32, "greenhouse_kernel_v1.1",
                                  GH_COLOR_BACKGROUND, GH_COLOR_BORDER_LIGHT, GH_COLOR_TEXT_PRIMARY,
                                  GH_COLOR_TEXT_MUTED, 1, 1);

        int badge_x = b1_x + inp_w + 12;
        renderer_draw_badge(badge_x, inp_y + 4, "PRODUCTION", GH_COLOR_GREEN_PILL_BG, GH_COLOR_GREEN_PILL_FG, 4);
        badge_x += font_text_width("PRODUCTION") + 22;
        renderer_draw_badge(badge_x, inp_y + 4, "ACTIVE", GH_COLOR_SURFACE_HOVER, GH_COLOR_TEXT_SECONDARY, 4);

        draw_text(c4_x + 14, c4_y + 184, "Consistent Greenhouse tokens: radii, active focus rings, and soft hovers",
                  GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

        /* ----------------------------------------------------------------------
         * Card 5: Bottom Full Width (Dedicated Greenhouse Cursor Family)
         * -------------------------------------------------------------------- */
        int c5_x = 24, c5_y = 528, c5_w = sw - 48, c5_h = 220;
        renderer_draw_glass_panel(c5_x, c5_y, c5_w, c5_h, 12, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 3);

        renderer_fill_aa_circle(c5_x + 18, c5_y + 18, 4, GH_COLOR_GREEN_LEAF, 255);
        draw_text(c5_x + 28, c5_y + 11, "GREENHOUSE CURSOR FAMILY SHOWCASE  (7 DEDICATED SHAPES)",
                  GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        draw_text(c5_x + 490, c5_y + 11, "Hover box or press keys 1-7 to switch cursor",
                  GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        renderer_fill_alpha_rect(c5_x + 12, c5_y + 32, c5_w - 24, 1, GH_COLOR_BORDER_LIGHT, 140);

        int cbox_w = (c5_w - 36 - 6 * 10) / 7;
        int cbox_h = 100;
        int cbox_y = c5_y + 42;

        for (int i = 0; i < CURSOR_TYPE_COUNT; i++) {
            int bx = c5_x + 18 + i * (cbox_w + 10);
            int is_active = (active_cursor_shape == i);
            int is_hover = (mx >= bx && mx < bx + cbox_w && my >= cbox_y && my < cbox_y + cbox_h);

            uint32_t bg_col = is_active ? 0xEDF7F1 : (is_hover ? GH_COLOR_SURFACE_HOVER : GH_COLOR_BACKGROUND);
            uint32_t brd_col = is_active ? GH_COLOR_GREEN_LEAF : (is_hover ? GH_COLOR_GREEN_SPROUT : GH_COLOR_BORDER_LIGHT);

            renderer_fill_aa_rounded_rect(bx, cbox_y, cbox_w, cbox_h, 8, bg_col, 255);
            renderer_draw_aa_rounded_rect(bx, cbox_y, cbox_w, cbox_h, 8, is_active ? 2 : 1, brd_col, 220);

            /* Number shortcut badge */
            char key_str[4];
            key_str[0] = '[';
            key_str[1] = '1' + i;
            key_str[2] = ']';
            key_str[3] = '\0';
            draw_text(bx + 8, cbox_y + 8, key_str, is_active ? GH_COLOR_GREEN_FOREST : GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

            /* Cursor icon sample: temporarily set shape to draw preview in box center */
            int orig_shape = cursor_get_shape();
            cursor_set_shape(i);
            int preview_cx = bx + cbox_w / 2;
            int preview_cy = cbox_y + 36;
            cursor_draw(preview_cx, preview_cy);
            cursor_set_shape(orig_shape);

            /* Cursor Name */
            const char* sname = cursor_shape_name(i);
            draw_centered_text(bx, cbox_y + 60, cbox_w, 14, sname, is_active ? GH_COLOR_GREEN_FOREST : GH_COLOR_TEXT_PRIMARY);

            /* Hotspot coordinates */
            int hx = 0, hy = 0;
            cursor_get_hotspot(i, &hx, &hy);
            char hot_buf[16];
            hot_buf[0] = '(';
            hot_buf[1] = '0' + (hx % 10);
            hot_buf[2] = ',';
            hot_buf[3] = ' ';
            hot_buf[4] = '0' + (hy % 10);
            hot_buf[5] = ')';
            hot_buf[6] = '\0';
            draw_centered_text(bx, cbox_y + 78, cbox_w, 14, hot_buf, GH_COLOR_TEXT_MUTED);
        }

        /* Status summary line at bottom of Card 5 */
        int st_y = c5_y + 154;
        renderer_fill_alpha_rect(c5_x + 12, st_y, c5_w - 24, 1, GH_COLOR_BORDER_LIGHT, 120);

        int hx = 0, hy = 0;
        cursor_get_hotspot(active_cursor_shape, &hx, &hy);

        char status_msg[128];
        int n = 0;
        const char* prefix = "Active: [";
        while (*prefix) status_msg[n++] = *prefix++;
        const char* cur_n = cursor_shape_name(active_cursor_shape);
        while (*cur_n && n < 40) status_msg[n++] = *cur_n++;
        const char* mid1 = "]  |  Hotspot: (";
        while (*mid1 && n < 60) status_msg[n++] = *mid1++;
        status_msg[n++] = '0' + (hx % 10);
        status_msg[n++] = ',';
        status_msg[n++] = ' ';
        status_msg[n++] = '0' + (hy % 10);
        const char* mid2 = ")  |  Pointer: (";
        while (*mid2 && n < 80) status_msg[n++] = *mid2++;

        /* Append mx, my */
        int temp_mx = mx;
        char x_buf[8]; int x_len = 0;
        if (temp_mx == 0) x_buf[x_len++] = '0';
        while (temp_mx > 0) { x_buf[x_len++] = '0' + (temp_mx % 10); temp_mx /= 10; }
        for (int k = x_len - 1; k >= 0; k--) status_msg[n++] = x_buf[k];
        status_msg[n++] = ','; status_msg[n++] = ' ';
        int temp_my = my;
        char y_buf[8]; int y_len = 0;
        if (temp_my == 0) y_buf[y_len++] = '0';
        while (temp_my > 0) { y_buf[y_len++] = '0' + (temp_my % 10); temp_my /= 10; }
        for (int k = y_len - 1; k >= 0; k--) status_msg[n++] = y_buf[k];

        const char* suffix = ")  |  Keys 1-7 switch cursor | +/- scale | ESC to exit";
        while (*suffix && n < 127) status_msg[n++] = *suffix++;
        status_msg[n] = '\0';

        draw_text(c5_x + 18, st_y + 10, status_msg, GH_COLOR_GREEN_FOREST, FONT_TRANSPARENT, 0);

        /* 5. Draw Software Cursor above every surface */
        cursor_tick();
        cursor_draw(mx, my);

        /* 6. Present Frame to Visible Framebuffer */
        graphics_present();
        graphics_end_frame();

        /* Halt CPU until next interrupt tick */
        asm volatile("hlt");
    }

    graphics_leave();
    return 0;
}
