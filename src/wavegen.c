#include "wavegen.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dsp.h"

const char *wg_name(int wave) {
    switch (wave) {
    case WG_SINE: return "sine";
    case WG_SQUARE: return "square";
    case WG_TRIANGLE: return "triangle";
    case WG_SAW: return "saw";
    case WG_LINSWEEP: return "lin-sweep";
    case WG_LOGSWEEP: return "log-sweep";
    case WG_MULTITONE: return "multi-tone";
    case WG_WHITE: return "white";
    case WG_PINK: return "pink";
    default: return "?";
    }
}

void wg_default(WgConfig *c, double fs) {
    memset(c, 0, sizeof(*c));
    c->wave = WG_SINE;
    c->fs = fs > 0 ? fs : 48000.0;
    c->f1 = c->f2 = 1000.0;
    c->amp = as_lin(-12.0);
    c->phase0 = 0.0;
    c->sweep_sec = 1.0;
    c->blep = 1;
    c->ntones = 1;
    c->tones[0] = 1000.0;
    c->tone_rel[0] = 1.0;
    c->seed = 0x9E3779B97F4A7C15ULL;
}

void wg_init(WgState *s, const WgConfig *c) {
    memset(s, 0, sizeof(*s));
    s->cfg = *c;
    if (s->cfg.fs <= 0) s->cfg.fs = 48000.0;
    if (s->cfg.f2 <= 0) s->cfg.f2 = s->cfg.f1;
    s->phase = s->cfg.phase0 - floor(s->cfg.phase0);
    s->rng = s->cfg.seed ? s->cfg.seed : 0x9E3779B97F4A7C15ULL;
}

uint64_t wg_rand(WgState *s) {
    uint64_t x = s->rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s->rng = x;
    return x * 2685821657736338717ULL;
}

static double rnd_bipolar(WgState *s) {
    return (double)(wg_rand(s) >> 11) * (2.0 / 9007199254740992.0) - 1.0;
}

static double polyblep(double t, double dt) {
    if (dt <= 0.0) return 0.0;
    if (t < dt) {
        double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt) {
        double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

/* 瞬时频率（Hz）：常数 / 线性扫频 / 对数扫频 */
static double inst_freq(const WgConfig *c, double t) {
    switch (c->wave) {
    case WG_LINSWEEP: {
        double T = c->sweep_sec > 1e-6 ? c->sweep_sec : 1e-6;
        double u = t / T;
        u -= floor(u);
        return c->f1 + (c->f2 - c->f1) * u;
    }
    case WG_LOGSWEEP: {
        double T = c->sweep_sec > 1e-6 ? c->sweep_sec : 1e-6;
        double u = t / T;
        u -= floor(u);
        double lo = c->f1 > 1e-6 ? c->f1 : 1e-6;
        double hi = c->f2 > lo ? c->f2 : lo * 1.0001;
        return lo * pow(hi / lo, u);
    }
    default:
        return c->f1;
    }
}

double wg_shape(int wave, double p, double inc, int blep) {
    switch (wave) {
    case WG_SINE:
        return sin(2.0 * AS_PI * p);
    case WG_SQUARE: {
        double sq = (p < 0.5) ? 1.0 : -1.0;
        if (blep) {
            double t2 = (p < 0.5) ? p + 0.5 : p - 0.5;
            sq += polyblep(p, inc);
            sq -= polyblep(t2, inc);
        }
        return sq;
    }
    case WG_SAW: {
        double saw = 2.0 * p - 1.0;
        if (blep) saw -= polyblep(p, inc);
        return saw;
    }
    case WG_TRIANGLE: {
        double t = p + 0.25;
        if (t >= 1.0) t -= 1.0;
        return 1.0 - 4.0 * fabs(t - 0.5);
    }
    default:
        return sin(2.0 * AS_PI * p);
    }
}

double wg_next(WgState *s) {
    WgConfig *c = &s->cfg;
    double fs = c->fs;
    double f = inst_freq(c, s->t);
    double inc = f / fs;
    if (inc > 0.5) inc = 0.5;              /* 超过 Nyquist 自动限幅，保护 */
    s->phase += inc;
    if (s->phase >= 1.0) s->phase -= floor(s->phase);
    s->t += 1.0 / fs;
    s->samples++;

    const double p = s->phase;
    double y = 0.0;
    switch (c->wave) {
    case WG_SINE:
    case WG_SQUARE:
    case WG_SAW:
    case WG_TRIANGLE:
    case WG_LINSWEEP:
    case WG_LOGSWEEP:
        y = wg_shape(c->wave, p, inc, c->blep);
        break;
    case WG_MULTITONE: {
        double sum = 0.0, norm = 0.0;
        int n = c->ntones;
        if (n < 1) n = 1;
        if (n > WG_MAX_TONES) n = WG_MAX_TONES;
        for (int i = 0; i < n; i++) {
            /* 每个分量用同一个相位累加器按频率比缩放，保证确定 */
            double ratio = (c->f1 > 1e-9) ? c->tones[i] / c->f1 : 1.0;
            double ph = fmod(p * ratio, 1.0);
            sum += c->tone_rel[i] * sin(2.0 * AS_PI * ph);
            norm += fabs(c->tone_rel[i]);
        }
        y = norm > 1e-9 ? sum / norm : 0.0;
        break;
    }
    case WG_WHITE:
        y = rnd_bipolar(s);
        break;
    case WG_PINK: {
        double w = rnd_bipolar(s);
        double *b = s->pink;
        b[0] = 0.99886 * b[0] + w * 0.0555179;
        b[1] = 0.99332 * b[1] + w * 0.0750759;
        b[2] = 0.96900 * b[2] + w * 0.1538520;
        b[3] = 0.86650 * b[3] + w * 0.3104856;
        b[4] = 0.55000 * b[4] + w * 0.5329522;
        b[5] = -0.7616 * b[5] - w * 0.0168980;
        double v = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362;
        b[6] = w * 0.115926;
        y = v * 0.16;
        break;
    }
    default:
        y = 0.0;
        break;
    }
    /* 幅度校准 + 削波保护 */
    y *= c->amp;
    if (y > 0.999) y = 0.999;
    if (y < -0.999) y = -0.999;
    return y;
}

void wg_render(WgState *s, float *dst, int frames) {
    for (int i = 0; i < frames; i++) dst[i] = (float)wg_next(s);
}

int wg_check(const WgConfig *c, int frames, WgCheck *out) {
    memset(out, 0, sizeof(*out));
    if (!c || frames < 2048) return -1;
    int n = 1024;
    while (n * 2 <= frames) n *= 2;              /* 取 2 的幂 */

    WgConfig cfg = *c;
    if (cfg.fs <= 0) cfg.fs = 48000.0;
    WgState st;
    wg_init(&st, &cfg);
    int settle = (int)(cfg.fs * 0.25);           /* 让积分器/噪声进入稳态 */
    for (int i = 0; i < settle; i++) wg_next(&st);

    float *buf = (float *)malloc((size_t)n * sizeof(float));
    double *x = (double *)malloc((size_t)n * sizeof(double));
    double *win = (double *)malloc((size_t)n * sizeof(double));
    double *amp = (double *)malloc((size_t)(n / 2 + 2) * sizeof(double));
    FFT fft;
    if (!buf || !x || !win || !amp || fft_init(&fft, n) != 0) {
        free(buf); free(x); free(win); free(amp);
        return -1;
    }
    wg_render(&st, buf, n);

    double peak = 0, sum = 0;
    for (int i = 0; i < n; i++) {
        double a = fabs(buf[i]);
        if (a > peak) peak = a;
        sum += buf[i];
        x[i] = buf[i];
    }
    out->dc = sum / n;
    out->amp_req_db = as_db(cfg.amp);
    out->amp_meas_db = as_db(peak);
    out->amp_err_db = out->amp_meas_db - out->amp_req_db;
    out->clipped = peak > 0.9985;

    int tonal = (cfg.wave <= WG_SAW) || (cfg.wave == WG_MULTITONE);
    out->freq_req = cfg.f1;
    if (tonal && fabs(cfg.f1 - cfg.f2) < 1e-9 && cfg.f1 > 1.0) {
        TimeStats ts;
        time_stats(buf, n, cfg.fs, 1.0, &ts);
        out->freq_meas = ts.freq;
        if (ts.freq > 0.0)
            out->freq_err_ppm = (ts.freq - cfg.f1) / cfg.f1 * 1e6;
        out->settled = 1;

        ToneMetrics tm;
        if (analyze_tone(&fft, x, n, cfg.fs, cfg.f1, WIN_BH, &tm)) {
            out->thd_percent = tm.thd * 100.0;
            /* 最强非谐波杂散：排除直流/基波/前 12 次谐波 */
            win_fill(WIN_BH, win, n);
            fft_spectrum(&fft, x, win, n, amp, n / 2);
            double bin_hz = cfg.fs / (double)n;
            double guard = (0.06 * cfg.f1 / bin_hz + 3.0) * bin_hz;
            if (guard < 3.0 * bin_hz) guard = 3.0 * bin_hz;
            double spur = 0.0;
            for (int k = 2; k < n / 2; k++) {
                double f = k * bin_hz;
                if (fabs(f - cfg.f1) <= guard) continue;
                int skip = 0;
                for (int h = 2; h <= 12; h++)
                    if (fabs(f - cfg.f1 * h) <= guard) { skip = 1; break; }
                if (skip) continue;
                if (amp[k] > spur) spur = amp[k];
            }
            double a1 = dft_at(x, win, n, cfg.f1, cfg.fs);
            out->spur_dbc = a1 > 0 ? as_db(spur / a1) : -180.0;
        }
    }
    fft_free(&fft);
    free(buf);
    free(x);
    free(win);
    free(amp);
    return 0;
}

