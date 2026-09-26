/* ==============================================================================
 * Greenhouse OS - VERDANT Morphing UI & Interpolation System
 * ==============================================================================
 */

#ifndef MORPH_H
#define MORPH_H

#include <stdint.h>

/* Step an integer value towards target by step_size (linear interpolation) */
int morph_step_linear(int current, int target, int step_size);

/* Step an integer value towards target with proportional easing */
int morph_step_ease(int current, int target, int divisor, int min_step);

/* Morph a 2D bounding rectangle smoothly towards target bounds */
int morph_rect(int* x, int* y, int* w, int* h,
               int tx, int ty, int tw, int th,
               int min_step);

#endif /* MORPH_H */
