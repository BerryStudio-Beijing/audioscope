#include "core/spectrogram.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int as_spectrogram_init(AsSpectrogram *s, int cols, int rows,
                        double fmin, double fmax) {
    if (!s || cols <= 0 || rows <= 0 || !(fmin > 0.0) || !(fmax > fmin))
        return -1;
    memset(s, 0, sizeof(*s));
    s->cols = cols;
    s->rows = rows;
    s->fmin = fmin;
    s->fmax = fmax;
    s->db = (float *)malloc(sizeof(float) * (size_t)cols * (size_t)rows);
    if (!s->db) return -1;
    as_spectrogram_clear(s);
    return 0;
}

void as_spectrogram_free(AsSpectrogram *s) {
    if (!s) return;
    free(s->db);
    memset(s, 0, sizeof(*s));
}

void as_spectrogram_clear(AsSpectrogram *s) {
    if (!s || !s->db) return;
    for (int i = 0; i < s->cols * s->rows; ++i) s->db[i] = -120.0f;
    s->head = 0;
    s->count = 0;
}

void as_spectrogram_push(AsSpectrogram *s, const double *mag, int nfft,
                         double fs) {
    if (!s || !s->db || !mag || nfft < 8 || !(fs > 0.0)) return;
    double maxv = 1e-12;
    for (int i = 1; i < nfft / 2; ++i) {
        if (mag[i] > maxv) maxv = mag[i];
    }
    int slot = s->head;
    for (int r = 0; r < s->rows; ++r) {
        double t = s->rows > 1 ? (double)r / (double)(s->rows - 1) : 0.0;
        double f = s->fmin * pow(s->fmax / s->fmin, t);
        int bin = (int)lround(f * (double)nfft / fs);
        if (bin < 1) bin = 1;
        if (bin > nfft / 2 - 1) bin = nfft / 2 - 1;
        double db = 20.0 * log10((mag[bin] > 1e-12 ? mag[bin] : 1e-12) / maxv);
        if (db < -120.0) db = -120.0;
        s->db[(size_t)slot * s->rows + r] = (float)db;
    }
    s->head = (s->head + 1) % s->cols;
    if (s->count < s->cols) ++s->count;
}

float as_spectrogram_get(const AsSpectrogram *s, int age, int row) {
    if (!s || !s->db || age < 0 || age >= s->count || row < 0 || row >= s->rows)
        return -120.0f;
    int start = (s->head - s->count + s->cols) % s->cols;
    int slot = (start + age) % s->cols;
    return s->db[(size_t)slot * s->rows + row];
}
