/* ==============================================================================
 * Greenhouse OS — Dedicated Renderer Visual Benchmark Screen
 * ==============================================================================
 * Visual benchmark demonstrating all rendering primitives:
 *   - Gradient background & ambient radial bloom
 *   - Physical frosted glass & translucency
 *   - Soft ambient multi-tier drop shadows
 *   - Antialiased rounded cards with subpixel coverage
 *   - Component system (buttons, badges, inputs, meters)
 *   - Complete cursor family showcase (all 7 cursor types)
 * ==============================================================================
 */

#ifndef GUI_TEST_SCREEN_H
#define GUI_TEST_SCREEN_H

#include <stdint.h>

/* Run the renderer visual benchmark screen.
 * max_seconds: 0 for interactive (ESC to exit), or positive timeout. */
int test_screen_run(int max_seconds);

#endif /* GUI_TEST_SCREEN_H */
