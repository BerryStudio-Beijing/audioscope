#include "core/window.h"

#include <math.h>

#define AS_PI 3.14159265358979323846

void as_window_hann(double *w, int n) {
    if (!w || n <= 1) return;
    for (int i = 0; i < n; ++i) {
        w[i] = 0.5 - 0.5 * cos(2.0 * AS_PI * (double)i / (double)(n - 1));
    }
}

void as_window_blackman(double *w, int n) {
    if (!w || n <= 1) return;
    for (int i = 0; i < n; ++i) {
        double x = 2.0 * AS_PI * (double)i / (double)(n - 1);
        w[i] = 0.42 - 0.5 * cos(x) + 0.08 * cos(2.0 * x);
    }
}

void as_window_rect(double *w, int n) {
    if (!w) return;
    for (int i = 0; i < n; ++i) w[i] = 1.0;
}
