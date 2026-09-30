/* ==============================================================================
 * Greenhouse OS — GHDesign System (Light Mode)
 *
 * Single source of truth for all visual design tokens.
 * Derived from the Greenhouse website: https://berrygreenhouse.freebuff.app
 * Adapted for a light-mode desktop operating system.
 * ==============================================================================
 */

#ifndef GH_THEME_H
#define GH_THEME_H

#include <stdint.h>

/* ==============================================================================
 * COLOR PALETTE (Botanical Emerald Glass)
 * Exact design tokens extracted from berrygreenhouse.vercel.app
 * ============================================================================== */

/* Base Neutrals & Botanical Canvas (Light Mode) */
#define GH_COLOR_WHITE              0xFFFFFFFF
#define GH_COLOR_OFF_WHITE          0xFFF9FBF9
#define GH_COLOR_BACKGROUND         0xFFF4F7F5   /* Pale botanical sage porcelain canvas */
#define GH_COLOR_BG_BASE            0xFFF4F7F5   /* Canvas top */
#define GH_COLOR_BG_GRAD_END        0xFFE2EBE5   /* Delicate sage bottom */
#define GH_COLOR_BG_BLOOM           0xFFC2E8D4   /* Luminous ambient botanical halo */
#define GH_COLOR_SURFACE            0xFFFFFFFF   /* Crisp luminous porcelain white glass */
#define GH_COLOR_SURFACE_GLASS      0xF4FFFFFF   /* Translucent physical frosted glass */
#define GH_COLOR_SURFACE_HOVER      0xFFF0F5F2   /* Subtle elevated surface */
#define GH_COLOR_SURFACE_PRESSED    0xFFE4EDE7
#define GH_COLOR_SURFACE_MUTED      0xFFEAF0EC

/* Text Hierarchy (--gh-ink tokens for Light Mode) */
#define GH_COLOR_TEXT_PRIMARY       0xFF122019   /* Rich botanical charcoal ink (AAA contrast) */
#define GH_COLOR_TEXT_SECONDARY     0xFF3E5246   /* Calm slate sage */
#define GH_COLOR_TEXT_MUTED         0xFF6B8074   /* Soft sage metadata */
#define GH_COLOR_TEXT_FAINT         0xFF9BB0A3   /* Subtle hints & hairline boundaries */
#define GH_COLOR_TEXT_INVERSE       0xFFFFFFFF   /* Crisp white */

/* Borders & Dividers */
#define GH_COLOR_BORDER_LIGHT       0xFFD6E2DA   /* Crisp botanical porcelain hairline */
#define GH_COLOR_BORDER_MEDIUM      0xFFC0D2C6   /* Slightly stronger card boundary */
#define GH_COLOR_BORDER_FOCUS       0xFF2D8A68   /* Vibrant brand green focus ring */
#define GH_COLOR_BORDER_GLINT       0x80FFFFFF   /* 1px top specular glint */
#define GH_COLOR_DIVIDER            0xFFE4EDE7

/* Brand Green Family */
#define GH_COLOR_GREEN_FOREST       0xFF134230   /* Deep botanical forest green */
#define GH_COLOR_GREEN_SPROUT       0xFF22B778   /* Bright accent mint */
#define GH_COLOR_GREEN_LEAF         0xFF2D8A68   /* Primary vibrant leaf */
#define GH_COLOR_GREEN_LEAF_DEEP    0xFF1B6E50   /* Vibrant brand */
#define GH_COLOR_GREEN_LEAF_SOFT    0xFFDCF3E7   /* Light rich emerald tint */
#define GH_COLOR_GREEN_HALO         0xFF9EE0C0   /* Luminous ambient halo */
#define GH_COLOR_GREEN_PILL_BG      0xFFDCF3E7   /* Botanical pill background */
#define GH_COLOR_GREEN_PILL_FG      0xFF156948   /* Botanical pill text */

/* Code & Terminal Canvas */
#define GH_COLOR_CODE_BG            0xFF0A140F   /* Obsidian code terminal (modern dark terminal on light desktop) */

/* Accent Colors */
#define GH_COLOR_AMBER_WARN         0xFFD97706   /* Warm amber warning */
#define GH_COLOR_RED_ERROR          0xFFDC2626   /* Error red */
#define GH_COLOR_BLUE_INFO          0xFF0284C7   /* Info cerulean blue */

/* Semantic UI Colors */
#define GH_COLOR_PRIMARY            0xFF2D8A68   /* Vibrant deep leaf for primary actions */
#define GH_COLOR_PRIMARY_FG         0xFFFFFFFF
#define GH_COLOR_SECONDARY          0xFFEAF1EC   /* Soft card surface */
#define GH_COLOR_SECONDARY_FG       0xFF122019
#define GH_COLOR_MUTED              0xFFF2F6F3
#define GH_COLOR_MUTED_FG           0xFF6B8074
#define GH_COLOR_DESTRUCTIVE        0xFFDC2626
#define GH_COLOR_DESTRUCTIVE_FG     0xFFFFFFFF
#define GH_COLOR_TRANSPARENT        0x00000000

/* Glass / Overlay */
#define GH_COLOR_GLASS_WHITE        0x40FFFFFF   /* 25% white overlay */
#define GH_COLOR_GLASS_BORDER       0x40B8D4C2   /* Crisp translucent border */
#define GH_COLOR_SHADOW_AMBIENT     0x12102418   /* Soft diffused shadow */
#define GH_COLOR_SHADOW_DEPTH       0x20102418   /* Deeper shadow */
#define GH_COLOR_SHADOW_ELEVATED    0x2E102418   /* Floating element shadow */

/* ==============================================================================
 * TYPOGRAPHY
 * Website: Instrument Serif (headings), Inter (UI), JetBrains Mono (code)
 * OS: We use the built-in 8x16 bitmap font for UI, but define hierarchy constants.
 * ============================================================================== */

#define GH_FONT_UI                  0   /* Built-in 8x16 for all UI text */
#define GH_FONT_MONO                0   /* Same font, monospace rendering for terminal */
#define GH_FONT_HEADING             0   /* Built-in, rendered larger via scaling if needed */

/* Font sizes (in logical units - actual rendering uses 8x16 glyphs) */
#define GH_TEXT_XS                  10   /* Small metadata */
#define GH_TEXT_SM                  12   /* Body small */
#define GH_TEXT_BASE                14   /* Body default */
#define GH_TEXT_LG                  16   /* Large body */
#define GH_TEXT_XL                  18   /* Small heading */
#define GH_TEXT_2XL                 20   /* Heading */
#define GH_TEXT_3XL                 24   /* Large heading */
#define GH_TEXT_4XL                 28   /* Display */

/* Font weights (simulated via rendering) */
#define GH_FONT_WEIGHT_NORMAL       400
#define GH_FONT_WEIGHT_MEDIUM       500
#define GH_FONT_WEIGHT_SEMIBOLD     600
#define GH_FONT_WEIGHT_BOLD         700

/* ==============================================================================
 * SPACING SYSTEM
 * Base unit: 4px (matching website --spacing: 0.25rem)
 * All values in pixels.
 * ============================================================================== */

#define GH_SPACE_0                  0
#define GH_SPACE_1                  4
#define GH_SPACE_2                  8
#define GH_SPACE_3                  12
#define GH_SPACE_4                  16
#define GH_SPACE_5                  20
#define GH_SPACE_6                  24
#define GH_SPACE_8                  32
#define GH_SPACE_10                 40
#define GH_SPACE_12                 48
#define GH_SPACE_16                 64
#define GH_SPACE_20                 80
#define GH_SPACE_24                 96

/* ==============================================================================
 * BORDER RADIUS
 * Base: 10px (website --radius: 0.625rem)
 * Derived scale matching website calc() tokens.
 * ============================================================================== */

#define GH_RADIUS_NONE              0
#define GH_RADIUS_XS                2    /* calc(var(--radius) - 8px) = 2px */
#define GH_RADIUS_SM                6    /* calc(var(--radius) - 4px) = 6px */
#define GH_RADIUS_MD                8    /* calc(var(--radius) - 2px) = 8px */
#define GH_RADIUS_LG                10   /* var(--radius) = 10px */
#define GH_RADIUS_XL                14   /* calc(var(--radius) + 4px) = 14px */
#define GH_RADIUS_2XL               16   /* var(--radius-2xl) = 16px */
#define GH_RADIUS_3XL               24   /* var(--radius-3xl) = 24px */
#define GH_RADIUS_FULL              9999 /* pill/rounded-full */

#define GH_RADIUS_DEFAULT           GH_RADIUS_LG
#define GH_RADIUS_CARD              GH_RADIUS_MD
#define GH_RADIUS_BUTTON            GH_RADIUS_MD
#define GH_RADIUS_INPUT             GH_RADIUS_MD
#define GH_RADIUS_WINDOW            GH_RADIUS_2XL
#define GH_RADIUS_PANEL             GH_RADIUS_XL

/* ==============================================================================
 * SHADOWS
 * Website has two systems: Tailwind gray shadows + brand-aware OKLCH shadows
 * We implement a simplified light-mode shadow scale.
 * ============================================================================== */

/* Shadow levels (alpha values for 0xAARRGGBB) */
#define GH_SHADOW_NONE              0x00000000
#define GH_SHADOW_SM                0x0A000000   /* 4% - subtle */
#define GH_SHADOW_DEFAULT           0x14000000   /* 8% - card default */
#define GH_SHADOW_MD                0x1E000000   /* 12% - elevated */
#define GH_SHADOW_LG                0x26000000   /* 15% - floating */
#define GH_SHADOW_XL                0x33000000   /* 20% - modal/dropdown */
#define GH_SHADOW_BRAND             0x3300D294   /* 20% brand green tint */

/* Shadow offsets (y, blur) */
#define GH_SHADOW_OFFSET_SM         1, 2
#define GH_SHADOW_OFFSET_DEFAULT    2, 4
#define GH_SHADOW_OFFSET_MD         4, 8
#define GH_SHADOW_OFFSET_LG         8, 16
#define GH_SHADOW_OFFSET_XL         12, 24

/* ==============================================================================
 * TRANSITIONS / MOTION
 * Website: --default-transition-duration: 0.15s, ease: cubic-bezier(0.4, 0, 0.2, 1)
 * Framer Motion defaults: duration: 0.3, ease: "easeInOut"
 * ============================================================================== */

#define GH_TRANSITION_FAST          100   /* ms */
#define GH_TRANSITION_DEFAULT       150   /* ms */
#define GH_TRANSITION_SLOW          300   /* ms */
#define GH_TRANSITION_AMBIENT       500   /* ms - for entrance animations */

/* Easing curves (for morph interpolation) */
#define GH_EASE_LINEAR              0
#define GH_EASE_IN                  1
#define GH_EASE_OUT                 2
#define GH_EASE_IN_OUT              3

/* ==============================================================================
 * COMPONENT HEIGHTS / SIZES
 * ============================================================================== */

#define GH_TOPBAR_HEIGHT            40
#define GH_DOCK_HEIGHT              56
#define GH_WINDOW_TITLE_HEIGHT      36
#define GH_BUTTON_HEIGHT_SM         28
#define GH_BUTTON_HEIGHT_MD         36
#define GH_BUTTON_HEIGHT_LG         44
#define GH_INPUT_HEIGHT             36
#define GH_CARD_PADDING             GH_SPACE_4
#define GH_CARD_GAP                 GH_SPACE_4

/* ==============================================================================
 * Z-INDEX LAYERS
 * ============================================================================== */

#define GH_Z_DESKTOP                0
#define GH_Z_WALLPAPER              1
#define GH_Z_WINDOW                 10
#define GH_Z_WINDOW_ACTIVE          20
#define GH_Z_DOCK                   50
#define GH_Z_TOPBAR                 60
#define GH_Z_MODAL                  100
#define GH_Z_LAUNCHER               200
#define GH_Z_TOOLTIP                300
#define GH_Z_CURSOR                 1000

/* ==============================================================================
 * HELPER MACROS
 * ============================================================================== */

static inline uint32_t gh_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFF000000) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static inline uint32_t gh_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static inline uint32_t gh_alpha_blend(uint32_t src, uint32_t dst, uint8_t alpha) {
    if (alpha == 255) return src | 0xFF000000;
    if (alpha == 0) return dst;

    uint32_t inv_a = 255 - alpha;
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t or = (sr * alpha + dr * inv_a) / 255;
    uint32_t og = (sg * alpha + dg * inv_a) / 255;
    uint32_t ob = (sb * alpha + db * inv_a) / 255;

    return (0xFF000000) | (or << 16) | (og << 8) | ob;
}

/* ==============================================================================
 * SEMANTIC COLOR ALIASES FOR COMPONENTS
 * ============================================================================== */

/* Window */
#define GH_WINDOW_BG                GH_COLOR_SURFACE
#define GH_WINDOW_BORDER            GH_COLOR_BORDER_LIGHT
#define GH_WINDOW_BORDER_FOCUS      GH_COLOR_GREEN_LEAF
#define GH_WINDOW_SHADOW            GH_SHADOW_LG
#define GH_WINDOW_TITLE_BG          GH_COLOR_SURFACE
#define GH_WINDOW_TITLE_FG          GH_COLOR_TEXT_PRIMARY
#define GH_WINDOW_ACCENT            GH_COLOR_GREEN_LEAF

/* Button variants */
#define GH_BTN_PRIMARY_BG           GH_COLOR_PRIMARY
#define GH_BTN_PRIMARY_FG           GH_COLOR_PRIMARY_FG
#define GH_BTN_PRIMARY_HOVER        0xFF00BD78
#define GH_BTN_PRIMARY_BORDER       GH_COLOR_PRIMARY

#define GH_BTN_SECONDARY_BG         GH_COLOR_SECONDARY
#define GH_BTN_SECONDARY_FG         GH_COLOR_SECONDARY_FG
#define GH_BTN_SECONDARY_HOVER      0xFFDCE7DF
#define GH_BTN_SECONDARY_BORDER     GH_COLOR_BORDER_LIGHT

#define GH_BTN_GHOST_BG             GH_COLOR_TRANSPARENT
#define GH_BTN_GHOST_FG             GH_COLOR_TEXT_PRIMARY
#define GH_BTN_GHOST_HOVER          GH_COLOR_SURFACE_HOVER
#define GH_BTN_GHOST_BORDER         GH_COLOR_TRANSPARENT

#define GH_BTN_OUTLINE_BG           GH_COLOR_TRANSPARENT
#define GH_BTN_OUTLINE_FG           GH_COLOR_TEXT_PRIMARY
#define GH_BTN_OUTLINE_HOVER        GH_COLOR_SURFACE_HOVER
#define GH_BTN_OUTLINE_BORDER       GH_COLOR_BORDER_LIGHT

#define GH_BTN_BRAND_BG             GH_COLOR_GREEN_LEAF_DEEP
#define GH_BTN_BRAND_FG             GH_COLOR_WHITE
#define GH_BTN_BRAND_HOVER          0xFF22B778
#define GH_BTN_BRAND_BORDER         GH_COLOR_GREEN_LEAF_DEEP

#define GH_BTN_DESTRUCTIVE_BG       GH_COLOR_DESTRUCTIVE
#define GH_BTN_DESTRUCTIVE_FG       GH_COLOR_DESTRUCTIVE_FG
#define GH_BTN_DESTRUCTIVE_HOVER    0xFFB91C1C
#define GH_BTN_DESTRUCTIVE_BORDER   GH_COLOR_DESTRUCTIVE

/* Input */
#define GH_INPUT_BG                 0xFFF7FAF8
#define GH_INPUT_FG                 GH_COLOR_TEXT_PRIMARY
#define GH_INPUT_PLACEHOLDER        GH_COLOR_TEXT_MUTED
#define GH_INPUT_BORDER             GH_COLOR_BORDER_LIGHT
#define GH_INPUT_BORDER_FOCUS       GH_COLOR_GREEN_LEAF
#define GH_INPUT_BORDER_ERROR       GH_COLOR_RED_ERROR

/* Card */
#define GH_CARD_BG                  GH_COLOR_SURFACE
#define GH_CARD_BORDER              GH_COLOR_BORDER_LIGHT
#define GH_CARD_SHADOW              GH_SHADOW_DEFAULT
#define GH_CARD_PADDING_VAL         GH_CARD_PADDING

/* Launcher */
#define GH_LAUNCHER_BG              GH_COLOR_SURFACE
#define GH_LAUNCHER_BORDER          GH_COLOR_BORDER_LIGHT
#define GH_LAUNCHER_SHADOW          GH_SHADOW_XL
#define GH_LAUNCHER_INPUT_BG        GH_COLOR_SURFACE_HOVER
#define GH_LAUNCHER_ITEM_HOVER      GH_COLOR_SURFACE_HOVER

/* Dock */
#define GH_DOCK_BG                  0xF4FFFFFF
#define GH_DOCK_BORDER              GH_COLOR_BORDER_LIGHT
#define GH_DOCK_SHADOW              GH_SHADOW_LG
#define GH_DOCK_ITEM_HOVER          GH_COLOR_SURFACE_HOVER
#define GH_DOCK_ITEM_ACTIVE_BG      GH_COLOR_GREEN_LEAF
#define GH_DOCK_INDICATOR           GH_COLOR_GREEN_LEAF

/* Topbar */
#define GH_TOPBAR_BG                0xF4FFFFFF
#define GH_TOPBAR_BORDER            GH_COLOR_BORDER_LIGHT
#define GH_TOPBAR_SHADOW            GH_SHADOW_SM
#define GH_TOPBAR_BRAND_BG          GH_COLOR_GREEN_LEAF_SOFT
#define GH_TOPBAR_BRAND_FG          GH_COLOR_GREEN_FOREST

/* Terminal */
#define GH_TERM_BG                  GH_COLOR_CODE_BG
#define GH_TERM_FG                  GH_COLOR_WHITE
#define GH_TERM_PROMPT              GH_COLOR_GREEN_SPROUT
#define GH_TERM_CURSOR              GH_COLOR_GREEN_SPROUT
#define GH_TERM_SELECTION_BG        0x4047D992   /* 25% brand green */
#define GH_TERM_SELECTION_FG        GH_COLOR_WHITE

/* Canvas Studio */
#define GH_CANVAS_BG                0xFFF4F7F5
#define GH_CANVAS_TOOLBAR_BG        GH_COLOR_SURFACE
#define GH_CANVAS_TOOLBAR_BORDER    GH_COLOR_BORDER_LIGHT

/* System Monitor */
#define GH_SYSMON_CPU_COLOR         GH_COLOR_GREEN_LEAF
#define GH_SYSMON_MEM_COLOR         GH_COLOR_BLUE_INFO
#define GH_SYSMON_HEAP_COLOR        GH_COLOR_AMBER_WARN

/* File Browser */
#define GH_FB_DIR_COLOR             GH_COLOR_AMBER_WARN
#define GH_FB_FILE_COLOR            GH_COLOR_BLUE_INFO

/* Berry Assistant */
#define GH_BERRY_BRAND              GH_COLOR_GREEN_HALO
#define GH_BERRY_ACCENT             GH_COLOR_GREEN_LEAF

/* ==============================================================================
 * ANIMATION TOKENS (for morph system)
 * ============================================================================== */

#define GH_ANIM_DURATION_ENTER      200
#define GH_ANIM_DURATION_EXIT       150
#define GH_ANIM_DURATION_HOVER      100
#define GH_ANIM_DURATION_DRAG       0
#define GH_ANIM_DURATION_RESIZE     0
#define GH_ANIM_DURATION_LAUNCHER   250

#endif /* GH_THEME_H */