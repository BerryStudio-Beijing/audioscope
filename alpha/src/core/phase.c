#include "core/phase.h"

#include "core/stats.h"

#include <math.h>
#include <stddef.h>

#define AS_PI 3.14159265358979323846

static double wrap180(double deg) {
    while (deg > 180.0) deg -= 360.0;
    while (deg < -180.0) deg += 360.0;
    return deg;
}

static void dft_at(const float *x, int n, double fs, double freq,
                   double *mag, double *phase_deg) {
    double re = 0.0, im = 0.0;
    double wsum = 0.0;
    for (int i = 0; i < n; ++i) {
        double w = n > 1 ? 0.5 - 0.5 * cos(2.0 * AS_PI * (double)i / (double)(n - 1)) : 1.0;
        double ph = 2.0 * AS_PI * freq * (double)i / fs;
        re += (double)x[i] * w * cos(ph);
        im -= (double)x[i] * w * sin(ph);
        wsum += w;
    }
    if (wsum > 0.0) wsum *= 0.5; /* coherent gain of Hann */
    *mag = wsum > 0.0 ? sqrt(re * re + im * im) / wsum : 0.0;
    *phase_deg = atan2(im, re) * 180.0 / AS_PI;
}

int as_phase_measure(const float *a, const float *b, int n, double fs,
                     AsPhaseResult *out) {
    if (!a || !b || !out || n < 32 || !(fs > 0.0)) return -1;
    AsStats st;
    as_stats_compute(a, n, fs, &st);
    if (!(st.freq > 0.0)) return -1;
    double ma = 0.0, mb = 0.0, pa = 0.0, pb = 0.0;
    dft_at(a, n, fs, st.freq, &ma, &pa);
    dft_at(b, n, fs, st.freq, &mb, &pb);
    if (!(ma > 0.0) || !(mb > 0.0)) return -1;
    out->freq_hz = st.freq;
    out->phase_deg = wrap180(pb - pa);
    out->gain_db = 20.0 * log10(mb / ma);
    out->coherence = 1.0;
    return 0;
}

int as_drift_estimate(const float *a, const float *b, int n, double fs,
                      int block, double *ppm, double *freq_hz) {
    if (!a || !b || !ppm || n < 256 || !(fs > 0.0)) return -1;
    if (block <= 0) block = n / 8;
    if (block < 256) block = 256;
    if (block > n / 2) block = n / 2;
    int count = n / block;
    if (count < 4) return -1;

    double sum_t = 0.0, sum_p = 0.0, sum_tt = 0.0, sum_tp = 0.0;
    double prev = 0.0;
    double freq = 0.0;
    int used = 0;
    for (int k = 0; k < count; ++k) {
        AsPhaseResult r;
        if (as_phase_measure(a + (size_t)k * block, b + (size_t)k * block,
                             block, fs, &r) != 0) {
            continue;
        }
        double p = r.phase_deg;
        if (used == 0) {
            prev = p;
        } else {
            while (p - prev > 180.0) p -= 360.0;
            while (p - prev < -180.0) p += 360.0;
        }
        prev = p;
        double t = ((double)k + 0.5) * (double)block / fs;
        sum_t += t;
        sum_p += p;
        sum_tt += t * t;
        sum_tp += t * p;
        if (used == 0) freq = r.freq_hz;
        ++used;
    }
    if (used < 4) return -1;
    double denom = (double)used * sum_tt - sum_t * sum_t;
    if (fabs(denom) < 1e-12) return -1;
    double slope = ((double)used * sum_tp - sum_t * sum_p) / denom; /* deg/s */
    double delta_f = slope / 360.0;
    if (freq_hz) *freq_hz = freq;
    *ppm = freq > 0.0 ? delta_f / freq * 1e6 : 0.0;
    return 0;
}
