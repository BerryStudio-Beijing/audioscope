#include "core/repair.h"

#include <math.h>
#include <stdlib.h>

int as_wow_measure(const float *x, int n, double fs, AsWowResult *out) {
    if (!x || !out || n < 1024 || !(fs > 0.0)) return -1;
    double *times = (double *)malloc(sizeof(double) * (size_t)(n / 2 + 1));
    if (!times) return -1;
    int count = 0;
    for (int i = 1; i < n; ++i) {
        double a = (double)x[i - 1];
        double b = (double)x[i];
        if (a <= 0.0 && b > 0.0) {
            times[count++] = (double)(i - 1) + (-a) / (b - a);
        }
    }
    if (count < 32) {
        free(times);
        return -1;
    }
    double fsum = 0.0;
    for (int i = 1; i < count; ++i) {
        double period = times[i] - times[i - 1];
        if (period > 0.0) fsum += fs / period;
    }
    double mean = fsum / (double)(count - 1);
    if (!(mean > 0.0)) {
        free(times);
        return -1;
    }

    double *dev = (double *)malloc(sizeof(double) * (size_t)(count - 1));
    if (!dev) {
        free(times);
        return -1;
    }
    for (int i = 1; i < count; ++i) {
        double period = times[i] - times[i - 1];
        double f = period > 0.0 ? fs / period : mean;
        dev[i - 1] = (f - mean) / mean;
    }
    /* One-pole split at 6 Hz: wow below, flutter above. */
    double dt = 1.0 / mean;
    double alpha = 1.0 - exp(-2.0 * 3.14159265358979323846 * 6.0 * dt);
    double lp = 0.0;
    double wow_sum = 0.0, flt_sum = 0.0;
    for (int i = 0; i < count - 1; ++i) {
        lp += alpha * (dev[i] - lp);
        double hp = dev[i] - lp;
        wow_sum += lp * lp;
        flt_sum += hp * hp;
    }
    int m = count - 1;
    out->wow_pct = 100.0 * sqrt(wow_sum / (double)m);
    out->flutter_pct = 100.0 * sqrt(flt_sum / (double)m);
    out->speed_ppm = (mean / 3000.0 - 1.0) * 1e6;
    out->freq_hz = mean;
    out->periods = m;
    free(dev);
    free(times);
    return 0;
}

int as_crosstalk_db(const float *a, const float *b, int n, double *db) {
    if (!a || !b || !db || n <= 0) return -1;
    double sa = 0.0, sb = 0.0;
    for (int i = 0; i < n; ++i) {
        sa += (double)a[i] * (double)a[i];
        sb += (double)b[i] * (double)b[i];
    }
    if (sa < 1e-18) return -1;
    double rmsa = sqrt(sa / (double)n);
    double rmsb = sqrt(sb / (double)n);
    double ratio = rmsb > 1e-12 ? rmsb / rmsa : 1e-12;
    *db = 20.0 * log10(ratio);
    return 0;
}

int as_impedance(double r_series, double v_dut, double v_source, double *z) {
    if (!z || !(r_series > 0.0) || !(v_source > 0.0) || v_dut < 0.0) return -1;
    if (v_dut >= v_source) return -1;
    *z = r_series * v_dut / (v_source - v_dut);
    return 0;
}
