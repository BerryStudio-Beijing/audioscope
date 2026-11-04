#include "analyze.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dsp.h"

double an_xcorr_delay(const float *tx, int ntx, const float *rx, int nrx,
                      double *peak) {
    if (peak) *peak = 0;
    if (!tx || !rx || ntx < 64 || nrx < ntx + 8) return 0.0;
    int win = ntx < 4096 ? ntx : 4096;
    double e_tx = 0;
    for (int i = 0; i < win; i++) e_tx += (double)tx[i] * tx[i];
    if (e_tx <= 1e-12) return 0.0;

    int max_lag = nrx - win - 1;
    double best = -1e30;
    int best_lag = 0;
    for (int lag = 0; lag <= max_lag; lag += 4) {
        double acc = 0, e_rx = 0;
        for (int i = 0; i < win; i++) {
            acc += (double)tx[i] * rx[lag + i];
            e_rx += (double)rx[lag + i] * rx[lag + i];
        }
        if (e_rx <= 1e-12) continue;
        double c = acc / sqrt(e_tx * e_rx);
        if (c > best) {
            best = c;
            best_lag = lag;
        }
    }
    int lo = best_lag - 4;
    int hi = best_lag + 4;
    if (lo < 0) lo = 0;
    if (hi > max_lag) hi = max_lag;
    for (int lag = lo; lag <= hi; lag++) {
        double acc = 0, e_rx = 0;
        for (int i = 0; i < win; i++) {
            acc += (double)tx[i] * rx[lag + i];
            e_rx += (double)rx[lag + i] * rx[lag + i];
        }
        if (e_rx <= 1e-12) continue;
        double c = acc / sqrt(e_tx * e_rx);
        if (c > best) {
            best = c;
            best_lag = lag;
        }
    }
    double frac = 0.0;
    if (best_lag > 0 && best_lag + win + 1 < nrx) {
        double a0 = 0, a1 = 0, a2 = 0, e0 = 0, e1 = 0, e2 = 0;
        for (int i = 0; i < win; i++) {
            a0 += (double)tx[i] * rx[best_lag - 1 + i];
            e0 += (double)rx[best_lag - 1 + i] * rx[best_lag - 1 + i];
            a1 += (double)tx[i] * rx[best_lag + i];
            e1 += (double)rx[best_lag + i] * rx[best_lag + i];
            a2 += (double)tx[i] * rx[best_lag + 1 + i];
            e2 += (double)rx[best_lag + 1 + i] * rx[best_lag + 1 + i];
        }
        double c0 = (e0 > 0) ? a0 / sqrt(e_tx * e0) : 0;
        double c1 = (e1 > 0) ? a1 / sqrt(e_tx * e1) : 0;
        double c2 = (e2 > 0) ? a2 / sqrt(e_tx * e2) : 0;
        double den = c0 - 2 * c1 + c2;
        if (fabs(den) > 1e-12) frac = 0.5 * (c0 - c2) / den;
        frac = AS_CLAMP(frac, -1.0, 1.0);
        best = c1 + 0.25 * (c0 - c2) * frac;
    }
    if (peak) *peak = best;
    return (double)best_lag + frac;
}

double an_tone(const float *x, int n, double fs, double f, double *phase_deg) {
    if (phase_deg) *phase_deg = 0;
    if (n < 256 || f <= 0 || fs <= 0) return 0;
    double *w = (double *)malloc((size_t)n * sizeof(double));
    if (!w) return 0;
    win_fill(WIN_BH, w, n);
    double sumr = 0, sumi = 0, wsum = 0;
    double step = -2.0 * AS_PI * f / fs;
    double c = cos(step), s = sin(step);
    double zr = 1.0, zi = 0.0;
    for (int i = 0; i < n; i++) {
        double v = x[i] * w[i];
        sumr += v * zr;
        sumi += v * zi;
        wsum += w[i];
        double nr = zr * c - zi * s;
        zi = zr * s + zi * c;
        zr = nr;
        if ((i & 255) == 255) {
            double m = sqrt(zr * zr + zi * zi);
            if (m > 0) {
                zr /= m;
                zi /= m;
            }
        }
    }
    free(w);
    if (wsum <= 0) return 0;
    double amp = 2.0 * sqrt(sumr * sumr + sumi * sumi) / wsum;
    if (phase_deg) {
        /* 约定：相位相对 sin() 而不是 cos()，直连时读数为 0° */
        double d = atan2(sumi, sumr) * 180.0 / AS_PI + 90.0;
        while (d > 180.0) d -= 360.0;
        while (d < -180.0) d += 360.0;
        *phase_deg = d;
    }
    return amp;
}

int an_sync_average(const float *x, int n, int period, float *out, int cap) {
    if (period < 8 || n < period * 2 || period > cap) return 0;
    int segs = n / period;
    if (segs > 512) segs = 512;
    for (int i = 0; i < period; i++) out[i] = 0.0f;
    for (int s = 0; s < segs; s++) {
        const float *src = x + (size_t)s * period;
        for (int i = 0; i < period; i++) out[i] += src[i];
    }
    for (int i = 0; i < period; i++) out[i] /= (float)segs;
    return period;
}

double an_noise_floor_dbfs(const float *x, int n) {
    if (n <= 0) return -180.0;
    double sum = 0, s2 = 0;
    for (int i = 0; i < n; i++) {
        sum += x[i];
        s2 += (double)x[i] * x[i];
    }
    double mean = sum / n;
    double var = s2 / n - mean * mean;
    if (var <= 1e-20) return -180.0;
    return 10.0 * log10(var);
}

double an_rise_time_us(const float *x, int n, double fs) {
    if (n < 64 || fs <= 0) return 0;
    float mn = x[0], mx = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] < mn) mn = x[i];
        if (x[i] > mx) mx = x[i];
    }
    double span = mx - mn;
    if (span < 1e-4) return 0;
    double lo = mn + 0.1 * span, hi = mn + 0.9 * span;
    double t_lo = -1;
    for (int i = 1; i < n; i++) {
        if (t_lo < 0 && x[i - 1] < lo && x[i] >= lo) {
            double a = x[i - 1], b = x[i];
            double f = (b != a) ? (lo - a) / (b - a) : 0;
            t_lo = (i - 1 + f) / fs;
        }
        if (t_lo >= 0 && x[i - 1] < hi && x[i] >= hi) {
            double a = x[i - 1], b = x[i];
            double f = (b != a) ? (hi - a) / (b - a) : 0;
            double t_hi = (i - 1 + f) / fs;
            return (t_hi - t_lo) * 1e6;
        }
    }
    return 0;
}

int an_fr_from_tones(const float *cap, int n, double fs, const double *freqs,
                     int nf, const double *stim_amp, FrPoint *out) {
    int k = 0;
    for (int i = 0; i < nf && k < AN_MAX_FR; i++) {
        double ph = 0;
        double amp = an_tone(cap, n, fs, freqs[i], &ph);
        if (amp <= 0) continue;
        double ref = stim_amp ? stim_amp[i] : 1.0;
        out[k].f = freqs[i];
        out[k].gain_raw_db = ref > 0 ? as_db(amp / ref) : 0;
        out[k].gain_db = out[k].gain_raw_db;
        out[k].phase_deg = ph;
        out[k].amp_err_db = as_db(amp) - (ref > 0 ? as_db(ref) : 0.0);
        k++;
    }
    return k;
}

int an_evaluate(const FrPoint *fr, int nfr, const CalibData *cal, AnResult *r,
                double tol_db) {
    memset(r, 0, sizeof(*r));
    if (nfr > 0) {
        double gmin = 1e9, gmax = -1e9, worst = 0, worst_f = 0;
        double ph_min = 1e9, ph_max = -1e9;
        for (int i = 0; i < nfr; i++) {
            double g = fr[i].gain_db;
            if (g < gmin) gmin = g;
            if (g > gmax) gmax = g;
            if (fabs(g) > fabs(worst)) {
                worst = g;
                worst_f = fr[i].f;
            }
            if (fr[i].phase_deg < ph_min) ph_min = fr[i].phase_deg;
            if (fr[i].phase_deg > ph_max) ph_max = fr[i].phase_deg;
        }
        r->flatness_db = gmax - gmin;
        r->max_dev_db = worst;
        r->max_dev_f = worst_f;
        r->phase_dev_deg = ph_max - ph_min;
        r->have_fr = 1;
        (void)cal;
        (void)tol_db;
    }
    return 0;
}

