#include "core/fft.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AS_PI 3.14159265358979323846

int as_fft_init(AsFft *fft, int n) {
    if (!fft || n < 2 || (n & (n - 1)) != 0) return -1;
    memset(fft, 0, sizeof(*fft));
    fft->n = n;
    fft->cos_tab = (double *)malloc(sizeof(double) * (size_t)(n / 2));
    fft->sin_tab = (double *)malloc(sizeof(double) * (size_t)(n / 2));
    fft->rev = (int *)malloc(sizeof(int) * (size_t)n);
    fft->re = (double *)malloc(sizeof(double) * (size_t)n);
    fft->im = (double *)malloc(sizeof(double) * (size_t)n);
    if (!fft->cos_tab || !fft->sin_tab || !fft->rev || !fft->re || !fft->im) {
        as_fft_free(fft);
        return -1;
    }
    for (int i = 0; i < n / 2; ++i) {
        fft->cos_tab[i] = cos(2.0 * AS_PI * (double)i / (double)n);
        fft->sin_tab[i] = sin(2.0 * AS_PI * (double)i / (double)n);
    }
    int bits = 0;
    while ((1 << bits) < n) ++bits;
    for (int i = 0; i < n; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) {
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        }
        fft->rev[i] = r;
    }
    return 0;
}

void as_fft_free(AsFft *fft) {
    if (!fft) return;
    free(fft->cos_tab);
    free(fft->sin_tab);
    free(fft->rev);
    free(fft->re);
    free(fft->im);
    memset(fft, 0, sizeof(*fft));
}

void as_fft_forward(AsFft *fft, const double *input, double *mag, int nfft) {
    if (!fft || !input || !mag || nfft != fft->n) return;
    int n = fft->n;
    for (int i = 0; i < n; ++i) {
        fft->re[i] = input[fft->rev[i]];
        fft->im[i] = 0.0;
    }
    for (int len = 2; len <= n; len <<= 1) {
        int half = len / 2;
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; ++j) {
                int k = j * step;
                double wr = fft->cos_tab[k];
                double wi = -fft->sin_tab[k];
                int a = i + j;
                int b = a + half;
                double tr = fft->re[b] * wr - fft->im[b] * wi;
                double ti = fft->re[b] * wi + fft->im[b] * wr;
                fft->re[b] = fft->re[a] - tr;
                fft->im[b] = fft->im[a] - ti;
                fft->re[a] += tr;
                fft->im[a] += ti;
            }
        }
    }
    for (int i = 0; i <= n / 2; ++i) {
        mag[i] = sqrt(fft->re[i] * fft->re[i] + fft->im[i] * fft->im[i]);
    }
}

int as_fft_peak_bin(const double *mag, int n) {
    if (!mag || n < 4) return 0;
    int best = 1;
    for (int i = 2; i < n / 2; ++i) {
        if (mag[i] > mag[best]) best = i;
    }
    return best;
}

double as_fft_bin_hz(int bin, int n, double fs) {
    return n > 0 ? (double)bin * fs / (double)n : 0.0;
}
