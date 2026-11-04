/* AudioScope C — DSP core (no dependencies beyond libm). */
#ifndef AS_DSP_H
#define AS_DSP_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#define AS_PI 3.14159265358979323846
#define AS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define AS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AS_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/* ---- small helpers ---------------------------------------------------- */
static inline double as_db(double amp) {
    double a = amp < 0 ? -amp : amp;
    if (a < 1e-9) a = 1e-9;
    return 20.0 * log10(a);
}
static inline double as_lin(double db) { return pow(10.0, db / 20.0); }

void as_eng(char *buf, size_t n, double v, const char *unit, int sig);
void as_fmt_time(char *buf, size_t n, double seconds);

/* ---- windows ---------------------------------------------------------- */
typedef enum { WIN_BH = 0, WIN_HANN, WIN_HAMMING, WIN_BLACKMAN, WIN_FLATTOP,
               WIN_RECT, WIN_COUNT } WinKind;
const char *win_label(WinKind k);
void win_fill(WinKind k, double *w, int n);

/* ---- FFT -------------------------------------------------------------- */
typedef struct {
    int n, shift;
    double *twr, *twi;
    int *rev;
    double *re, *im;   /* work buffers, length n */
} FFT;

int  fft_init(FFT *f, int n);
void fft_free(FFT *f);
void fft_forward(FFT *f, double *re, double *im);
/* magnitude of x (length n) windowed, nfft >= n; amp[0..nfft/2] peak amps */
void fft_spectrum(FFT *f, const double *x, const double *w, int n,
                  double *amp, int nfft);
/* windowed single bin DFT (Goertzel via phasor recurrence) */
double dft_at(const double *x, const double *w, int n, double freq, double fs);
/* complex single bin DFT of a float signal with a rectangular window */
void dft_complex_at(const float *x, int n, double freq, double fs,
                    double *re, double *im);

/* ---- generator -------------------------------------------------------- */
enum { W_SINE = 0, W_SQUARE, W_TRI, W_SAW, W_WHITE, W_PINK, W_DUAL, W_CHIRP,
       W_SILENCE, W_COUNT };

typedef struct {
    int wave;
    double freq, amp_db, offset;
    int enabled;
    int ch_en[2];
    double ch_phase[2];
    int ch_inv[2];
    double dual_freq;
    double sweep_f1, sweep_f2, sweep_sec;
} GenCfg;

typedef struct {
    double fs;
    double phase[2], phase2[2], gain[2];
    double sweep_pos;
    uint64_t rng;
    double pink[2][7];
    uint64_t samples;      /* 已生成的帧数（用于与输出帧号对齐） */
    uint64_t start_frame;  /* samples == 0 时对应的输出帧号 */
} Gen;

void gen_init(Gen *g, double fs);
void gen_free(Gen *g);
void gen_reset(Gen *g);
/* renders `frames` interleaved stereo samples */
void gen_render(Gen *g, const GenCfg *cfg, float *out, int frames);

/* ---- trigger ---------------------------------------------------------- */
double find_trigger(const float *y, int n, double level, int falling,
                    double hyst, int start);
double auto_level(const float *y, int n);

/* ---- time domain stats ------------------------------------------------ */
typedef struct {
    double vpp, vmax, vmin, mean, rms, ac_rms;
    double freq, period, crest, duty;
    int clipped;
} TimeStats;

void time_stats(const float *x, int n, double fs, double scale, TimeStats *st);

/* ---- single tone metrics --------------------------------------------- */
typedef struct {
    double freq, amp, amp_db, rms;
    double thd, thd_db, thdn, thdn_db;
    double snr, sinad, enob, sfdr;
    double harm[10];
} ToneMetrics;

int analyze_tone(FFT *fft, const double *x, int n, double fs, double hint,
                 WinKind wk, ToneMetrics *tm);

/* ---- log frequency binning ------------------------------------------- */
void log_bins(const double *freqs, const double *amps, int n, int cols,
              double fmin, double fmax, double *out);

#endif
