/* AudioScope by BerryStudio 0.6-alpha — iterative radix-2 FFT (C11, no dependencies). */
#ifndef AS_CORE_FFT_H
#define AS_CORE_FFT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n;
    double *cos_tab;
    double *sin_tab;
    int *rev;
    double *re;
    double *im;
} AsFft;

int  as_fft_init(AsFft *fft, int n);
void as_fft_free(AsFft *fft);
void as_fft_forward(AsFft *fft, const double *input, double *mag, int nfft);

/* Returns the bin index of the largest magnitude in [1, n/2). */
int  as_fft_peak_bin(const double *mag, int n);
double as_fft_bin_hz(int bin, int n, double fs);

#ifdef __cplusplus
}
#endif

#endif
