#include "core/tone.h"

#include "core/fft.h"
#include "core/window.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AS_PI 3.14159265358979323846

static int next_pow2_at_least(int n) {
    int p = 8192;
    while (p < n && p < 65536) p <<= 1;
    return p;
}

static int peak_near(const double *mag, int nfft, double fs, double f,
                     double tolerance) {
    int lo = (int)floor((f * (1.0 - tolerance)) * nfft / fs);
    int hi = (int)ceil((f * (1.0 + tolerance)) * nfft / fs);
    if (lo < 2) lo = 2;
    if (hi > nfft / 2 - 2) hi = nfft / 2 - 2;
    int best = lo;
    for (int i = lo; i <= hi; ++i) {
        if (mag[i] > mag[best]) best = i;
    }
    return best;
}

static double bin_hz(int bin, int nfft, double fs) {
    return (double)bin * fs / (double)nfft;
}

static int analyze_mag(const double *mag, int nfft, double fs,
                       double expected_f, int *peak_out, double *fund_power,
                       double *harm_power, double *total_power,
                       double *noise_power, double *max_spur) {
    int peak = peak_near(mag, nfft, fs, expected_f, 0.08);
    if (peak < 2 || peak >= nfft / 2 - 1) return -1;
    double fp = 0.0;
    for (int i = peak - 4; i <= peak + 4; ++i) fp += mag[i] * mag[i];
    double total = 0.0;
    for (int i = 1; i < nfft / 2; ++i) total += mag[i] * mag[i];
    double hp = 0.0;
    double f0 = bin_hz(peak, nfft, fs);
    for (int h = 2; h <= 10; ++h) {
        double fh = f0 * (double)h;
        if (fh >= fs / 2.0 - 10.0) break;
        int bh = (int)lround(fh * nfft / fs);
        for (int i = bh - 3; i <= bh + 3; ++i) {
            if (i > 0 && i < nfft / 2) hp += mag[i] * mag[i];
        }
    }
    double noise = 0.0;
    for (int i = 1; i < nfft / 2; ++i) {
        int guard = (i >= peak - 8 && i <= peak + 8);
        if (!guard) {
            for (int h = 2; h <= 10; ++h) {
                double fh = f0 * (double)h;
                if (fh >= fs / 2.0 - 10.0) break;
                int bh = (int)lround(fh * nfft / fs);
                if (i >= bh - 8 && i <= bh + 8) { guard = 1; break; }
            }
        }
        if (!guard) noise += mag[i] * mag[i];
    }
    double spur = 0.0;
    for (int i = 2; i < nfft / 2 - 1; ++i) {
        if (i >= peak - 3 && i <= peak + 3) continue;
        double v = mag[i] * mag[i];
        if (v > spur) spur = v;
    }
    *peak_out = peak;
    *fund_power = fp;
    *harm_power = hp;
    *total_power = total;
    *noise_power = noise;
    *max_spur = sqrt(spur);
    return 0;
}

int as_tone_analyze(const float *x, int n, double fs, AsToneResult *out) {
    if (!x || !out || n < 2048 || !(fs > 0.0)) return -1;
    int nfft = next_pow2_at_least(n);
    AsFft fft;
    if (as_fft_init(&fft, nfft) != 0) return -1;
    double *win = (double *)malloc(sizeof(double) * (size_t)nfft);
    double *in = (double *)malloc(sizeof(double) * (size_t)nfft);
    double *mag = (double *)malloc(sizeof(double) * (size_t)(nfft / 2 + 1));
    if (!win || !in || !mag) {
        free(win); free(in); free(mag);
        as_fft_free(&fft);
        return -1;
    }
    int win_n = n < nfft ? n : nfft;
    for (int i = 0; i < nfft; ++i) in[i] = i < win_n ? (double)x[i] : 0.0;
    as_window_hann(win, win_n);
    double sumw = 0.0;
    for (int i = 0; i < win_n; ++i) {
        in[i] *= win[i];
        sumw += win[i];
    }
    as_fft_forward(&fft, in, mag, nfft);

    /* Find the dominant tone globally, then measure around it. */
    int peak = 1;
    for (int i = 2; i < nfft / 2 - 1; ++i) if (mag[i] > mag[peak]) peak = i;
    double f0 = bin_hz(peak, nfft, fs);
    int peak2 = 0;
    double fp = 0.0, hp = 0.0, total = 0.0, noise = 0.0, spur = 0.0;
    if (analyze_mag(mag, nfft, fs, f0, &peak2, &fp, &hp, &total, &noise, &spur) != 0) {
        free(win); free(in); free(mag); as_fft_free(&fft);
        return -1;
    }
    if (noise < 1e-18) noise = 1e-18;
    if (fp < 1e-18) fp = 1e-18;
    out->freq_hz = bin_hz(peak2, nfft, fs);
    out->fundamental = sumw > 0.0 ? 2.0 * sqrt(fp) / sumw : 0.0;
    out->thd_pct = 100.0 * sqrt(hp / fp);
    out->thd_db = 10.0 * log10(hp / fp);
    out->thdn_pct = 100.0 * sqrt((hp + noise) / fp);
    out->thdn_db = 10.0 * log10((hp + noise) / fp);
    out->snr_db = 10.0 * log10(fp / noise);
    out->sinad_db = 10.0 * log10(fp / (hp + noise));
    out->sfdr_db = spur > 0.0 ? 20.0 * log10(sqrt(fp) / spur) : 0.0;
    out->enob = (out->sinad_db - 1.76) / 6.02;

    free(win); free(in); free(mag);
    as_fft_free(&fft);
    return 0;
}

int as_imd_analyze(const float *x, int n, double fs, int din,
                   double *imd_pct, double *f1_out, double *f2_out) {
    if (!x || !imd_pct || n < 4096 || !(fs > 0.0)) return -1;
    double f1 = din ? 250.0 : 60.0;
    double f2 = din ? 8000.0 : 7000.0;
    int nfft = next_pow2_at_least(n);
    AsFft fft;
    if (as_fft_init(&fft, nfft) != 0) return -1;
    double *win = (double *)malloc(sizeof(double) * (size_t)nfft);
    double *in = (double *)malloc(sizeof(double) * (size_t)nfft);
    double *mag = (double *)malloc(sizeof(double) * (size_t)(nfft / 2 + 1));
    if (!win || !in || !mag) {
        free(win); free(in); free(mag); as_fft_free(&fft);
        return -1;
    }
    int win_n = n < nfft ? n : nfft;
    for (int i = 0; i < nfft; ++i) in[i] = i < win_n ? (double)x[i] : 0.0;
    as_window_hann(win, win_n);
    for (int i = 0; i < win_n; ++i) in[i] *= win[i];
    as_fft_forward(&fft, in, mag, nfft);

    int b2 = peak_near(mag, nfft, fs, f2, 0.05);
    if (b2 < 2) {
        free(win); free(in); free(mag); as_fft_free(&fft);
        return -1;
    }
    double carrier = mag[b2];
    if (carrier < 1e-12) {
        free(win); free(in); free(mag); as_fft_free(&fft);
        return -1;
    }
    double side = 0.0;
    for (int k = 1; k <= 5; ++k) {
        for (int sign = -1; sign <= 1; sign += 2) {
            double f = f2 + sign * (double)k * f1;
            if (f < 20.0 || f > fs / 2.0 - 20.0) continue;
            int b = (int)lround(f * (double)nfft / fs);
            for (int i = b - 2; i <= b + 2; ++i) {
                if (i > 0 && i < nfft / 2) side += mag[i] * mag[i];
            }
        }
    }
    *imd_pct = 100.0 * sqrt(side) / carrier;
    if (f1_out) *f1_out = f1;
    if (f2_out) *f2_out = f2;
    free(win); free(in); free(mag);
    as_fft_free(&fft);
    return 0;
}
