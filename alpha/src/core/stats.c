#include "core/stats.h"

#include <math.h>

double as_linear_to_db(double amp) {
    double a = fabs(amp);
    if (a < 1e-12) a = 1e-12;
    return 20.0 * log10(a);
}

void as_stats_compute(const float *x, int n, double fs, AsStats *out) {
    if (!out) return;
    out->peak = out->vpp = out->rms = out->mean = out->freq = 0.0;
    out->zero_crossings = 0;
    if (!x || n <= 1 || fs <= 0.0) return;

    double sum = 0.0;
    double peak = 0.0;
    double sumsq = 0.0;
    for (int i = 0; i < n; ++i) {
        double v = (double)x[i];
        sum += v;
        sumsq += v * v;
        double a = fabs(v);
        if (a > peak) peak = a;
    }
    out->mean = sum / (double)n;
    out->rms = sqrt(sumsq / (double)n);
    out->peak = peak;
    out->vpp = 2.0 * peak;

    /* Positive-going zero crossings with linear interpolation. */
    double first = -1.0;
    double last = -1.0;
    int count = 0;
    for (int i = 1; i < n; ++i) {
        double a = (double)x[i - 1] - out->mean;
        double b = (double)x[i] - out->mean;
        if (a <= 0.0 && b > 0.0) {
            double t = (double)(i - 1) + (-a) / (b - a);
            if (first < 0.0) first = t;
            else last = t;
            ++count;
        }
    }
    out->zero_crossings = count;
    if (first >= 0.0 && last > first && count >= 2) {
        double period = (last - first) / (double)(count - 1);
        if (period > 0.0) out->freq = fs / period;
    }
}
