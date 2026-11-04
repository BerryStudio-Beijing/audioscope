#include "dsp.h"
#include "wavegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- formatting ------------------------------------------------------- */
void as_eng(char *buf, size_t n, double v, const char *unit, int sig) {
    static const char *pre[] = {"G", "M", "k", "", "m", "u", "n", "p"};
    if (!isfinite(v)) { snprintf(buf, n, "-"); return; }
    if (v == 0.0) {
        if (unit && *unit) snprintf(buf, n, "0 %s", unit);
        else snprintf(buf, n, "0");
        return;
    }
    double av = fabs(v);
    int ex = (int)floor(log10(av) / 3.0) * 3;
    if (ex > 9) ex = 9;
    if (ex < -12) ex = -12;
    double scaled = v / pow(10.0, ex);
    if (fabs(scaled) >= 999.9995) { ex += 3; scaled = v / pow(10.0, ex); }
    int idx = (9 - ex) / 3;
    const char *p = (idx >= 0 && idx < 8) ? pre[idx] : "";
    char num[32];
    snprintf(num, sizeof(num), "%.*g", sig, scaled);
    if (strchr(num, 'e') || strchr(num, 'E')) {
        /* rounding pushed us to 1000 -> shift one prefix instead */
        if (ex < 9) {
            ex += 3;
            scaled = v / pow(10.0, ex);
            idx = (9 - ex) / 3;
            p = (idx >= 0 && idx < 8) ? pre[idx] : "";
            snprintf(num, sizeof(num), "%.*g", sig, scaled);
        }
        if (strchr(num, 'e') || strchr(num, 'E'))
            snprintf(num, sizeof(num), "%.0f", scaled);
    }
    if (unit && *unit) snprintf(buf, n, "%s %s%s", num, p, unit);
    else snprintf(buf, n, "%s %s", num, p);
}

void as_fmt_time(char *buf, size_t n, double seconds) {
    if (!isfinite(seconds) || seconds < 0) seconds = 0;
    int s = (int)(seconds + 0.5);
    snprintf(buf, n, "%02d:%02d", s / 60, s % 60);
}

/* ---- FFT (iterative radix-2) ------------------------------------------ */
int fft_init(FFT *f, int n) {
    memset(f, 0, sizeof(*f));
    int shift = 0;
    while ((1 << shift) < n) shift++;
    if ((1 << shift) != n || n < 8) return -1;
    f->n = n;
    f->shift = shift;
    f->rev = (int *)malloc((size_t)n * sizeof(int));
    f->twr = (double *)malloc((size_t)n / 2 * sizeof(double));
    f->twi = (double *)malloc((size_t)n / 2 * sizeof(double));
    f->re = (double *)malloc((size_t)n * sizeof(double));
    f->im = (double *)malloc((size_t)n * sizeof(double));
    if (!f->rev || !f->twr || !f->twi || !f->re || !f->im) return -1;
    for (int i = 0; i < n; i++) {
        unsigned x = (unsigned)i, r = 0;
        for (int b = 0; b < shift; b++) { r = (r << 1) | (x & 1u); x >>= 1; }
        f->rev[i] = (int)r;
    }
    for (int i = 0; i < n / 2; i++) {
        double a = -2.0 * AS_PI * (double)i / (double)n;
        f->twr[i] = cos(a);
        f->twi[i] = sin(a);
    }
    return 0;
}

void fft_free(FFT *f) {
    free(f->rev); free(f->twr); free(f->twi); free(f->re); free(f->im);
    memset(f, 0, sizeof(*f));
}

void fft_forward(FFT *f, double *re, double *im) {
    int n = f->n;
    int *rev = f->rev;
    for (int i = 0; i < n; i++) {
        int j = rev[i];
        if (j > i) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            int k = 0;
            for (int j = 0; j < half; j++, k += step) {
                double wr = f->twr[k], wi = f->twi[k];
                int a = i + j, b = a + half;
                double tr = re[b] * wr - im[b] * wi;
                double ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
}

void fft_spectrum(FFT *f, const double *x, const double *w, int n,
                  double *amp, int nfft) {
    int nf = f->n;
    double wsum = 0.0;
    for (int i = 0; i < n; i++) wsum += w[i];
    if (wsum <= 0.0) wsum = (double)n;
    for (int i = 0; i < nf; i++) {
        double v = (i < n) ? x[i] * w[i] : 0.0;
        f->re[i] = v;
        f->im[i] = 0.0;
    }
    fft_forward(f, f->re, f->im);
    int half = nf / 2;
    if (nfft < half) half = nfft;
    for (int k = 0; k <= half; k++) {
        double mag = sqrt(f->re[k] * f->re[k] + f->im[k] * f->im[k]);
        double a = 2.0 * mag / wsum;
        if (k == 0) a *= 0.5;
        amp[k] = a;
    }
}

double dft_at(const double *x, const double *w, int n, double freq, double fs) {
    if (n < 8 || freq <= 0.0 || fs <= 0.0) return 0.0;
    double wsum = 0.0;
    for (int i = 0; i < n; i++) wsum += w[i];
    if (wsum <= 0.0) return 0.0;
    double step = -2.0 * AS_PI * freq / fs;
    double c = cos(step), s = sin(step);
    double zr = 1.0, zi = 0.0, sumr = 0.0, sumi = 0.0;
    for (int i = 0; i < n; i++) {
        double v = x[i] * w[i];
        sumr += v * zr;
        sumi += v * zi;
        double nr = zr * c - zi * s;
        zi = zr * s + zi * c;
        zr = nr;
        if ((i & 255) == 255) {          /* renormalise against drift */
            double m = sqrt(zr * zr + zi * zi);
            if (m > 0) { zr /= m; zi /= m; }
        }
    }
    return 2.0 * sqrt(sumr * sumr + sumi * sumi) / wsum;
}

const char *win_label(WinKind k) {
    switch (k) {
    case WIN_BH: return "Blackman-Harris";
    case WIN_HANN: return "Hann";
    case WIN_HAMMING: return "Hamming";
    case WIN_BLACKMAN: return "Blackman";
    case WIN_FLATTOP: return "Flat-top";
    default: return "Rect";
    }
}

void win_fill(WinKind k, double *w, int n) {
    if (n <= 0) return;
    if (k == WIN_RECT || n == 1) {
        for (int i = 0; i < n; i++) w[i] = 1.0;
        return;
    }
    double d = (double)(n - 1);
    for (int i = 0; i < n; i++) {
        double t = (double)i / d;
        double a = 2.0 * AS_PI * t;
        switch (k) {
        case WIN_HANN: w[i] = 0.5 - 0.5 * cos(a); break;
        case WIN_HAMMING: w[i] = 0.54 - 0.46 * cos(a); break;
        case WIN_BLACKMAN:
            w[i] = 0.42 - 0.5 * cos(a) + 0.08 * cos(2 * a);
            break;
        case WIN_FLATTOP:
            w[i] = 0.21557895 - 0.41663158 * cos(a) + 0.277263158 * cos(2 * a)
                   - 0.083578947 * cos(3 * a) + 0.006947368 * cos(4 * a);
            break;
        default: /* Blackman-Harris 4 term */
            w[i] = 0.35875 - 0.48829 * cos(a) + 0.14128 * cos(2 * a)
                   - 0.01168 * cos(3 * a);
            break;
        }
    }
}

/* ---- generator -------------------------------------------------------- */
void gen_init(Gen *g, double fs) {
    memset(g, 0, sizeof(*g));
    g->fs = fs > 0 ? fs : 48000.0;
    g->rng = 0x2545F4914F6CDD1DULL;
}

void gen_free(Gen *g) { (void)g; }

void gen_reset(Gen *g) {
    double fs = g->fs;
    uint64_t rng = g->rng;
    memset(g, 0, sizeof(*g));
    g->fs = fs;
    g->rng = rng ? rng : 0x2545F4914F6CDD1DULL;
}

static double rnd_bipolar(Gen *g) {
    uint64_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    g->rng = x;
    return (double)(x >> 11) * (2.0 / 9007199254740992.0) - 1.0;
}

/* 波形核心已统一到 wavegen.c（wg_shape），此处不再保留第二份实现 */

static double pink_step(Gen *g, int ch) {
    double w = rnd_bipolar(g);
    double *b = g->pink[ch];
    b[0] = 0.99886 * b[0] + w * 0.0555179;
    b[1] = 0.99332 * b[1] + w * 0.0750759;
    b[2] = 0.96900 * b[2] + w * 0.1538520;
    b[3] = 0.86650 * b[3] + w * 0.3104856;
    b[4] = 0.55000 * b[4] + w * 0.5329522;
    b[5] = -0.7616 * b[5] - w * 0.0168980;
    double y = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362;
    b[6] = w * 0.115926;
    return y * 0.22;
}

void gen_render(Gen *g, const GenCfg *c, float *out, int frames) {
    if (frames <= 0) return;
    memset(out, 0, (size_t)frames * 2 * sizeof(float));
    if (!c->enabled || c->wave == W_SILENCE) {
        g->gain[0] = g->gain[1] = 0.0;
        return;
    }
    const double fs = g->fs;
    const double amp = as_lin(c->amp_db);
    double period = c->sweep_sec > 1e-3 ? c->sweep_sec : 1e-3;
    double f1 = c->sweep_f1 < 1e-3 ? 1e-3 : c->sweep_f1;
    double f2 = c->sweep_f2 <= f1 ? f1 * 1.0001 : c->sweep_f2;
    double ratio = f2 / f1;
    double sweep0 = g->sweep_pos;

    for (int ch = 0; ch < 2; ch++) {
        double target = c->ch_en[ch] ? amp : 0.0;
        double g0 = g->gain[ch];
        double ph = g->phase[ch];
        double ph2 = g->phase2[ch];
        double sweep = sweep0;
        double off = c->ch_phase[ch] / 360.0;
        double inc2 = (c->freq > 1e-6) ? (c->dual_freq / c->freq) : 1.0;

        for (int i = 0; i < frames; i++) {
            double inc;
            if (c->wave == W_CHIRP) {
                sweep += 1.0 / fs;
                if (sweep >= period) sweep -= period;
                inc = f1 * pow(ratio, sweep / period) / fs;
            } else {
                inc = c->freq / fs;
            }
            ph += inc;
            if (ph >= 1.0) ph -= 1.0;
            double p = ph + off;
            if (p >= 1.0) p -= 1.0;
            if (p < 0.0) p += 1.0;

            double y;
            switch (c->wave) {
            case W_SQUARE:
                y = wg_shape(WG_SQUARE, p, inc, 1);
                break;
            case W_SAW:
                y = wg_shape(WG_SAW, p, inc, 1);
                break;
            case W_TRI:
                y = wg_shape(WG_TRIANGLE, p, inc, 1);
                break;
            case W_WHITE:
                y = rnd_bipolar(g);
                break;
            case W_PINK:
                y = pink_step(g, ch);
                break;
            case W_DUAL: {
                ph2 += inc * inc2;
                if (ph2 >= 1.0) ph2 -= 1.0;
                y = 0.5 * (sin(2.0 * AS_PI * p) + sin(2.0 * AS_PI * ph2));
                break;
            }
            default:
                y = wg_shape(WG_SINE, p, inc, 1);
                break;
            }
            if (c->ch_inv[ch]) y = -y;

            double gain = g0 + (target - g0) * (double)(i + 1) / (double)frames;
            double gate = amp > 1e-12 ? gain / amp : 0.0;
            double v = y * gain + c->offset * gate;
            if (v > 1.0) v = 1.0;
            if (v < -1.0) v = -1.0;
            out[2 * i + ch] = (float)v;
        }
        g->gain[ch] = target;
        g->phase[ch] = ph;
        g->phase2[ch] = ph2;
        if (ch == 0) g->sweep_pos = sweep;
    }
    g->samples += (uint64_t)frames;
}

/* ---- trigger ---------------------------------------------------------- */
double find_trigger(const float *y, int n, double level, int falling,
                    double hyst, int start) {
    if (n < 2) return -1.0;
    if (start < 0) start = 0;
    int armed = 0;
    for (int i = start; i < n - 1; i++) {
        double a = (double)y[i] - level;
        double b = (double)y[i + 1] - level;
        if (falling) {
            if (a >= hyst) armed = 1;
        } else {
            if (a <= -hyst) armed = 1;
        }
        int cross = falling ? (a > 0.0 && b <= 0.0) : (a <= 0.0 && b > 0.0);
        if (!cross) continue;
        if (hyst > 0.0 && !armed) continue;
        double frac = (a == b) ? 0.0 : (-a / (b - a));
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        return (double)i + frac;
    }
    return -1.0;
}

double auto_level(const float *y, int n) {
    if (n <= 0) return 0.0;
    double mn = y[0], mx = y[0];
    for (int i = 1; i < n; i++) {
        if (y[i] < mn) mn = y[i];
        if (y[i] > mx) mx = y[i];
    }
    return 0.5 * (mn + mx);
}

/* ---- time domain stats ------------------------------------------------ */
void time_stats(const float *x, int n, double fs, double scale, TimeStats *st) {
    memset(st, 0, sizeof(*st));
    if (n <= 0 || fs <= 0) return;
    double sum = 0.0, sum2 = 0.0, mn = x[0], mx = x[0];
    for (int i = 0; i < n; i++) {
        double v = x[i];
        sum += v;
        sum2 += v * v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    double mean = sum / n;
    double rms = sqrt(sum2 / n);
    double ac2 = 0.0, peak = 0.0;
    for (int i = 0; i < n; i++) {
        double d = x[i] - mean;
        ac2 += d * d;
        double ad = fabs(d);
        if (ad > peak) peak = ad;
    }
    double ac_rms = sqrt(ac2 / n);

    st->mean = mean * scale;
    st->rms = rms * scale;
    st->ac_rms = ac_rms * scale;
    st->vmax = mx * scale;
    st->vmin = mn * scale;
    st->vpp = (mx - mn) * scale;
    st->crest = ac_rms > 1e-12 ? (0.5 * (mx - mn)) / ac_rms : 0.0;
    st->clipped = (fabs(mx) > 0.999 || fabs(mn) > 0.999);

    if (ac_rms > 1e-9) {
        double thr = 0.05 * peak;
        int count = 0;
        double t_first = 0, t_last = 0, pos_sum = 0;
        for (int i = 0; i < n - 1; i++) {
            double a = x[i] - mean, b = x[i + 1] - mean;
            if (a > 0) pos_sum += 1.0;
            if (a <= -thr && b > -thr) {
                double frac = (-thr - a) / (b - a);
                if (frac < 0) frac = 0;
                if (frac > 1) frac = 1;
                double t = ((double)i + frac) / fs;
                if (count == 0) t_first = t;
                t_last = t;
                count++;
            }
        }
        st->duty = pos_sum / (double)n;
        if (count >= 2) {
            double span = t_last - t_first;
            if (span > 1e-9) {
                st->freq = (double)(count - 1) / span;
                st->period = 1.0 / st->freq;
            }
        }
    }
}

/* ---- single tone metrics --------------------------------------------- */
static double parab_delta(double a, double b, double c) {
    double la = log(AS_MAX(a, 1e-20));
    double lb = log(AS_MAX(b, 1e-20));
    double lc = log(AS_MAX(c, 1e-20));
    double den = la - 2.0 * lb + lc;
    if (fabs(den) < 1e-12) return 0.0;
    double d = 0.5 * (la - lc) / den;
    return AS_CLAMP(d, -0.5, 0.5);
}

int analyze_tone(FFT *fft, const double *x, int n, double fs, double hint,
                 WinKind wk, ToneMetrics *tm) {
    memset(tm, 0, sizeof(*tm));
    if (n < 512 || fs <= 0 || fft == NULL) return 0;
    int nfft = fft->n;
    if (n > nfft) return 0;

    double *mem = (double *)malloc((size_t)(2 * n + nfft / 2 + 2) * sizeof(double));
    if (!mem) return 0;
    double *w = mem;
    double *y = mem + n;
    double *amp = mem + 2 * n;

    win_fill(wk, w, n);
    double wsum = 0.0, msum = 0.0;
    for (int i = 0; i < n; i++) { wsum += w[i]; msum += x[i] * w[i]; }
    if (wsum <= 0) { free(mem); return 0; }
    double mean = msum / wsum;
    double p_total = 0.0;
    for (int i = 0; i < n; i++) {
        y[i] = x[i] - mean;
        p_total += y[i] * y[i];
    }
    p_total /= n;
    if (p_total <= 1e-24) { free(mem); return 1; }   /* silence */

    fft_spectrum(fft, y, w, n, amp, nfft / 2);
    double bin_hz = fs / (double)nfft;
    int kmin = (int)(20.0 / bin_hz);
    if (kmin < 1) kmin = 1;
    int kmax = nfft / 2 - 1;
    int kbest = -1;
    double best = 0.0;
    for (int k = kmin; k <= kmax; k++) {
        if (amp[k] > best) { best = amp[k]; kbest = k; }
    }
    if (kbest < 0 || best <= 0.0) { free(mem); return 0; }
    double est = (double)kbest * bin_hz;
    if (kbest > 1 && kbest < kmax)
        est += parab_delta(amp[kbest - 1], amp[kbest], amp[kbest + 1]) * bin_hz;

    double f0 = est;
    if (hint > 0.0) {
        if (fabs(est - hint) <= AS_MAX(0.05 * hint, 5.0)) f0 = hint;
    }
    if (f0 <= 0.0 || f0 >= fs * 0.5) { free(mem); return 0; }

    double a1 = dft_at(y, w, n, f0, fs);
    if (a1 <= 1e-12) { free(mem); return 1; }

    int nh = 0;
    for (int k = 2; k <= 10; k++) {
        double fk = f0 * k;
        if (fk >= fs * 0.49) break;
        tm->harm[nh++] = dft_at(y, w, n, fk, fs);
    }
    double hpow = 0.0, hsum = 0.0;
    for (int i = 0; i < nh; i++) { hpow += tm->harm[i] * tm->harm[i]; }
    hsum = sqrt(hpow);

    double p_fund = a1 * a1 * 0.5;
    double p_harm = hpow * 0.5;

    /* 精确残余：先在时域最小二乘拟合基波，再取残差。
       直接用 p_total 减 p_fund 会受“非整数周期”误差影响（曾把 THD+N 压成 0）。 */
    double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
    for (int i = 0; i < n; i++) {
        double ph = 2.0 * AS_PI * f0 * (double)i / fs;
        double s = sin(ph), c = cos(ph);
        ss += s * s;
        cc += c * c;
        sc += s * c;
        ys += y[i] * s;
        yc += y[i] * c;
    }
    double det = ss * cc - sc * sc;
    double ca = 0, cb = 0;
    if (fabs(det) > 1e-12) {
        ca = (ys * cc - yc * sc) / det;
        cb = (yc * ss - ys * sc) / det;
    }
    double resid2 = 0;
    for (int i = 0; i < n; i++) {
        double ph = 2.0 * AS_PI * f0 * (double)i / fs;
        double d = y[i] - (ca * sin(ph) + cb * cos(ph));
        resid2 += d * d;
    }
    resid2 /= n;
    double p_fit = (ca * ca + cb * cb) * 0.5;
    if (p_fit <= 0) p_fit = p_fund;
    double p_noise = resid2 - p_harm;
    if (p_noise < p_fit * 1e-15) p_noise = p_fit * 1e-15;
    if (resid2 < p_fit * 1e-15) resid2 = p_fit * 1e-15;

    tm->freq = f0;
    tm->amp = a1;
    tm->amp_db = as_db(a1);
    tm->rms = sqrt(p_total);
    tm->thd = nh > 0 ? hsum / a1 : 0.0;
    tm->thd_db = as_db(tm->thd);
    tm->thdn = sqrt(resid2 / p_fit);
    tm->thdn_db = as_db(tm->thdn);
    tm->sinad = 10.0 * log10(p_fit / resid2);
    tm->snr = 10.0 * log10(p_fit / p_noise);
    tm->enob = (tm->sinad - 1.76) / 6.02;

    /* SFDR: strongest spur away from DC, the fundamental and its harmonics */
    double guard = AS_MAX(3.0, 0.06 * f0 / bin_hz + 2.0);
    double spur = 0.0;
    for (int k = (int)(2.0 * bin_hz / bin_hz); k <= kmax; k++) {
        double f = (double)k * bin_hz;
        if (f < AS_MAX(20.0, 2.0 * bin_hz)) continue;
        if (fabs(f - f0) <= guard * bin_hz) continue;
        int skip = 0;
        for (int h = 2; h <= 12; h++) {
            if (fabs(f - f0 * h) <= guard * bin_hz) { skip = 1; break; }
        }
        if (skip) continue;
        if (amp[k] > spur) spur = amp[k];
    }
    tm->sfdr = spur > 0.0 ? as_db(a1 / spur) : 0.0;

    free(mem);
    return 1;
}

void dft_complex_at(const float *x, int n, double freq, double fs,
                    double *re, double *im) {
    *re = 0;
    *im = 0;
    if (n < 8 || freq <= 0.0 || fs <= 0.0) return;
    double step = -2.0 * AS_PI * freq / fs;
    double c = cos(step), s = sin(step);
    double zr = 1.0, zi = 0.0, sumr = 0.0, sumi = 0.0;
    for (int i = 0; i < n; i++) {
        double v = x[i];
        sumr += v * zr;
        sumi += v * zi;
        double nr = zr * c - zi * s;
        zi = zr * s + zi * c;
        zr = nr;
        if ((i & 255) == 255) {
            double m = sqrt(zr * zr + zi * zi);
            if (m > 0) { zr /= m; zi /= m; }
        }
    }
    *re = 2.0 * sumr / (double)n;
    *im = 2.0 * sumi / (double)n;
}

/* ---- log frequency binning ------------------------------------------- */
void log_bins(const double *freqs, const double *amps, int n, int cols,
              double fmin, double fmax, double *out) {
    if (cols < 2) return;
    if (fmin < 1e-3) fmin = 1e-3;
    if (fmax < fmin * 1.5) fmax = fmin * 1.5;
    for (int j = 0; j < cols; j++) out[j] = 1e-9;
    if (n < 4) return;
    double lr = log(fmax / fmin);
    for (int j = 0; j < cols; j++) {
        double fa = fmin * exp(lr * (double)j / (double)(cols - 1));
        double fb = fmin * exp(lr * (double)(j + 1) / (double)(cols - 1));
        if (j == cols - 1) fb = fa * 1.0001;
        /* linear scan (freqs is sorted) */
        int a = 0;
        while (a < n && freqs[a] < fa) a++;
        int b = a;
        while (b < n && freqs[b] < fb) b++;
        if (b <= a) b = a + 1;
        if (a >= n) break;
        double m = 0.0;
        for (int k = a; k < b && k < n; k++) if (amps[k] > m) m = amps[k];
        out[j] = m;
    }
}

