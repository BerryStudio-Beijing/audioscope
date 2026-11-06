/* AudioScope by BerryStudio 0.5-alpha — time × frequency dB matrix. */
#ifndef AS_CORE_SPECTROGRAM_H
#define AS_CORE_SPECTROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int cols;
    int rows;
    float *db;      /* cols × rows, newest written at head-1 */
    int head;
    int count;
    double fmin;
    double fmax;
} AsSpectrogram;

int  as_spectrogram_init(AsSpectrogram *s, int cols, int rows,
                         double fmin, double fmax);
void as_spectrogram_free(AsSpectrogram *s);
void as_spectrogram_clear(AsSpectrogram *s);
void as_spectrogram_push(AsSpectrogram *s, const double *mag, int nfft,
                         double fs);
/* age 0 = oldest retained frame, rows 0 = fmin. Returns dB or -120. */
float as_spectrogram_get(const AsSpectrogram *s, int age, int row);

#ifdef __cplusplus
}
#endif

#endif
