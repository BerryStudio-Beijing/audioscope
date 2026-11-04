/* Self tests for the DSP core: ./audioscope --selftest */
#include "../src/dsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char *what, double got, double want) {
    if (cond) {
        printf("  ok   %-40s got %12.4f\n", what, got);
    } else {
        printf("  FAIL %-40s got %12.4f  want %12.4f\n", what, got, want);
        failures++;
    }
}

static void check_near(const char *what, double got, double want, double tol) {
    check(fabs(got - want) <= tol, what, got, want);
}

static void fill_sine(float *x, int n, double fs, double f, double amp) {
    for (int i = 0; i < n; i++)
        x[i] = (float)(amp * sin(2.0 * AS_PI * f * (double)i / fs));
}

static void test_generator(void) {
    printf("[generator]\n");
    double fs = 48000.0;
    Gen g;
    gen_init(&g, fs);
    GenCfg c;
    memset(&c, 0, sizeof(c));
    c.wave = W_SINE;
    c.freq = 1000.0;
    c.amp_db = -6.0;
    c.enabled = 1;
    c.ch_en[0] = 1;

    int frames = 48000;
    float *buf = (float *)malloc((size_t)frames * 2 * sizeof(float));
    gen_render(&g, &c, buf, frames);
    double peak = 0;
    for (int i = 0; i < frames; i++) {
        double a = fabs(buf[2 * i]);
        if (a > peak) peak = a;
    }
    check_near("sine peak at -6 dBFS", peak, as_lin(-6.0), 0.005);

    TimeStats st;
    float *mono = (float *)malloc(24000 * sizeof(float));
    for (int i = 0; i < 24000; i++) mono[i] = buf[2 * (24000 + i)];
    time_stats(mono, 24000, fs, 1.0, &st);
    free(mono);
    check_near("measured frequency", st.freq, 1000.0, 1.0);

    c.wave = W_SQUARE;
    gen_reset(&g);
    gen_render(&g, &c, buf, frames);
    double *d = (double *)malloc(8192 * sizeof(double));
    for (int i = 0; i < 8192; i++) d[i] = buf[2 * (frames - 8192) + 2 * i];
    FFT fft;
    fft_init(&fft, 16384);
    ToneMetrics tm;
    analyze_tone(&fft, d, 8192, fs, 1000.0, WIN_BH, &tm);
    check_near("square THD (%)", tm.thd * 100.0, 43.0, 3.0);
    check_near("square H3 (dBc)", as_db(tm.harm[1] / tm.amp), -9.54, 2.0);
    fft_free(&fft);
    free(d);

    c.wave = W_SINE;
    gen_reset(&g);
    c.amp_db = -6.0;
    gen_render(&g, &c, buf, 256);
    float before = buf[2 * 255];
    c.amp_db = -40.0;
    gen_render(&g, &c, buf, 256);
    check(fabs((double)buf[0] - before) < 0.05, "no click on level change",
          (double)buf[0] - before, 0.0);
    free(buf);
    gen_free(&g);
}

static void test_spectrum(void) {
    printf("[spectrum]\n");
    double fs = 48000.0;
    int n = 16384;
    double *x = (double *)malloc(n * sizeof(double));
    for (int i = 0; i < n; i++)
        x[i] = 0.5 * sin(2.0 * AS_PI * 1000.0 * (double)i / fs);

    FFT fft;
    fft_init(&fft, n);
    double *w = (double *)malloc(n * sizeof(double));
    double *amp = (double *)malloc((n / 2 + 1) * sizeof(double));
    win_fill(WIN_BH, w, n);
    fft_spectrum(&fft, x, w, n, amp, n / 2);
    int kbest = 0;
    for (int k = 1; k < n / 2; k++) if (amp[k] > amp[kbest]) kbest = k;
    double fpk = (double)kbest * fs / n;
    check_near("FFT peak frequency", fpk, 1000.0, fs / n);
    check_near("FFT peak level (dBFS)", as_db(amp[kbest]), -6.0, 1.0);
    check_near("DFT amplitude (dBFS)", as_db(dft_at(x, w, n, 1000.0, fs)), -6.0, 0.05);

    const WinKind wins[] = {WIN_BH, WIN_HANN, WIN_HAMMING, WIN_BLACKMAN,
                            WIN_FLATTOP, WIN_RECT};
    double worst = 0;
    for (int i = 0; i < 6; i++) {
        win_fill(wins[i], w, n);
        double a = dft_at(x, w, n, 1000.0, fs);
        double err = fabs(as_db(a) - as_db(0.5));
        if (err > worst) worst = err;
    }
    check(worst < 0.15, "window amplitude consistency (dB)", worst, 0.0);

    ToneMetrics tm;
    /* use a bin-coherent tone so the residual reflects distortion, not the
       partial cycle at the end of the record */
    double fc = 341.0 * fs / (double)n;
    for (int i = 0; i < n; i++)
        x[i] = 0.5 * sin(2.0 * AS_PI * fc * (double)i / fs);
    analyze_tone(&fft, x, n, fs, fc, WIN_BH, &tm);
    check_near("pure sine THD (%)", tm.thd * 100.0, 0.0, 0.001);
    check(tm.sinad > 90.0, "pure sine SINAD (dB)", tm.sinad, 90.0);
    check_near("tone frequency", tm.freq, fc, 0.5);

    free(w); free(amp); free(x);
    fft_free(&fft);
}

static void test_tone_metrics(void) {
    printf("[tone metrics]\n");
    double fs = 48000.0;
    int n = 16384;
    double *x = (double *)malloc(n * sizeof(double));
    double f0 = 1000.0, a1 = 0.5, a2 = 0.5 * as_lin(-40.0);
    for (int i = 0; i < n; i++) {
        double t = (double)i / fs;
        x[i] = a1 * sin(2 * AS_PI * f0 * t) + a2 * sin(2 * AS_PI * 2 * f0 * t);
    }
    FFT fft;
    fft_init(&fft, n);
    ToneMetrics tm;
    analyze_tone(&fft, x, n, fs, f0, WIN_BH, &tm);
    check_near("known THD (%)", tm.thd * 100.0, 1.0, 0.02);
    check_near("known THD (dB)", tm.thd_db, -40.0, 0.2);
    fft_free(&fft);
    free(x);
}

static void test_trigger(void) {
    printf("[trigger]\n");
    double fs = 48000.0;
    int n = 4096;
    float *y = (float *)malloc(n * sizeof(float));
    for (int i = 0; i < n; i++)
        y[i] = (float)sin(2 * AS_PI * 1000.0 * (double)i / fs + 0.7);
    double pos = find_trigger(y, n, 0.0, 0, 0.02, 0);
    double expect = (2 * AS_PI - 0.7) / (2 * AS_PI) * 1e-3 * fs;
    check_near("rising trigger index", pos, expect, 1.0);

    double fpos = find_trigger(y, n, 0.0, 1, 0.02, 0);
    check(fpos > 0 && fpos < n, "falling trigger found", fpos, 0.0);

    for (int i = 0; i < n; i++) y[i] = (float)(0.002 * sin((double)i * 12.9898));
    double noisy = find_trigger(y, n, 0.0, 0, 0.05, 0);
    check(noisy < 0, "hysteresis rejects noise", noisy, -1.0);

    double lvl = auto_level(y, n);
    check(fabs(lvl) < 0.01, "auto level near zero", lvl, 0.0);
    free(y);
}

static void test_time_stats(void) {
    printf("[time stats]\n");
    double fs = 48000.0;
    int n = 48000;
    float *x = (float *)malloc(n * sizeof(float));
    fill_sine(x, n, fs, 500.0, 0.25);
    TimeStats st;
    time_stats(x, n, fs, 2.0, &st);
    check_near("vpp scaled", st.vpp, 1.0, 0.01);
    check_near("rms scaled", st.ac_rms, 0.25 * 2.0 / sqrt(2.0), 0.01);
    check_near("frequency", st.freq, 500.0, 0.5);
    check_near("period", st.period, 0.002, 0.00002);
    check_near("duty", st.duty, 0.5, 0.01);
    check(st.clipped == 0, "no clipping flag", (double)st.clipped, 0.0);
    free(x);
}

static void test_logbins(void) {
    printf("[log bins]\n");
    int n = 8192, cols = 128;
    double *freqs = (double *)malloc(n * sizeof(double));
    double *amps = (double *)malloc(n * sizeof(double));
    double *out = (double *)malloc(cols * sizeof(double));
    double fs = 48000.0;
    for (int i = 0; i < n; i++) { freqs[i] = (double)i * fs / (2.0 * n); amps[i] = 1e-6; }
    amps[(int)(1000.0 / (fs / (2.0 * n)))] = 0.5;
    log_bins(freqs, amps, n, cols, 20.0, fs / 2, out);
    int k = 0;
    for (int j = 1; j < cols; j++) if (out[j] > out[k]) k = j;
    double fbin = 20.0 * pow((fs / 2) / 20.0, (double)k / (cols - 1));
    check_near("log bin peak frequency", fbin, 1000.0, 150.0);
    check_near("log bin peak level", out[k], 0.5, 0.01);
    free(freqs); free(amps); free(out);
}

static void test_format(void) {
    printf("[format]\n");
    char b[64];
    as_eng(b, sizeof(b), 0.001, "s", 3);
    check(strcmp(b, "1 ms") == 0, "eng 1 ms", 1.0, 1.0);
    as_eng(b, sizeof(b), 1000.0, "Hz", 3);
    check(strcmp(b, "1 kHz") == 0, "eng 1 kHz", 2.0, 2.0);
    as_eng(b, sizeof(b), 0.00015, "s", 3);
    check(strcmp(b, "150 us") == 0, "eng 150 us", 3.0, 3.0);
    as_fmt_time(b, sizeof(b), 125.0);
    check(strcmp(b, "02:05") == 0, "time 02:05", 4.0, 4.0);
}

int run_selftest(void) {
    failures = 0;
    printf("AudioScope C self test\n=====================\n");
    test_format();
    test_generator();
    test_spectrum();
    test_tone_metrics();
    test_trigger();
    test_time_stats();
    test_logbins();
    printf("=====================\n%s (%d failures)\n",
           failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}

