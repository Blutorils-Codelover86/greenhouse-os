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

/* Step an opacity value (0..255) towards target with linear/ease step */
uint8_t morph_step_opacity(uint8_t current, uint8_t target, int step_size);

/* Morph a 2D bounding rectangle smoothly towards target bounds */
int morph_rect(int* x, int* y, int* w, int* h,
               int tx, int ty, int tw, int th,
               int min_step);

/* Morph 2D bounds and opacity together */
int morph_surface_transition(int* x, int* y, int* w, int* h, uint8_t* opacity,
                             int tx, int ty, int tw, int th, uint8_t target_opacity,
                             int min_step);

#endif /* MORPH_H */
