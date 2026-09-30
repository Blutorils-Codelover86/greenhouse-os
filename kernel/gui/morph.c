/* ==============================================================================
 * Greenhouse OS - VERDANT Morphing UI & Interpolation System (implementation)
 * ==============================================================================
 */

#include "morph.h"

int morph_step_linear(int current, int target, int step_size) {
    if (step_size <= 0) step_size = 1;
    if (current < target) {
        current += step_size;
        if (current > target) current = target;
    } else if (current > target) {
        current -= step_size;
        if (current < target) current = target;
    }
    return current;
}

int morph_step_ease(int current, int target, int divisor, int min_step) {
    if (divisor <= 0) divisor = 4;
    if (min_step <= 0) min_step = 2;

    int diff = target - current;
    if (diff == 0) return target;

    int step = diff / divisor;
    if (diff > 0 && step < min_step) step = min_step;
    if (diff < 0 && step > -min_step) step = -min_step;

    current += step;
    if ((diff > 0 && current > target) || (diff < 0 && current < target)) {
        current = target;
    }
    return current;
}

int morph_rect(int* x, int* y, int* w, int* h,
               int tx, int ty, int tw, int th,
               int min_step) {
    int changed = 0;
    if (x && *x != tx) { *x = morph_step_ease(*x, tx, 4, min_step); changed = 1; }
    if (y && *y != ty) { *y = morph_step_ease(*y, ty, 4, min_step); changed = 1; }
    if (w && *w != tw) { *w = morph_step_ease(*w, tw, 4, min_step); changed = 1; }
    if (h && *h != th) { *h = morph_step_ease(*h, th, 4, min_step); changed = 1; }
    return changed;
}

uint8_t morph_step_opacity(uint8_t current, uint8_t target, int step_size) {
    if (step_size <= 0) step_size = 8;
    if (current < target) {
        int next = (int)current + step_size;
        return (next > (int)target) ? target : (uint8_t)next;
    } else if (current > target) {
        int next = (int)current - step_size;
        return (next < (int)target) ? target : (uint8_t)next;
    }
    return target;
}

int morph_surface_transition(int* x, int* y, int* w, int* h, uint8_t* opacity,
                             int tx, int ty, int tw, int th, uint8_t target_opacity,
                             int min_step) {
    int changed = morph_rect(x, y, w, h, tx, ty, tw, th, min_step);
    if (opacity && *opacity != target_opacity) {
        *opacity = morph_step_opacity(*opacity, target_opacity, 12);
        changed = 1;
    }
    return changed;
}

