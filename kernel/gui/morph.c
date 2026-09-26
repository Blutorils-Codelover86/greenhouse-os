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
