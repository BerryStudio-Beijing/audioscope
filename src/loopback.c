#include "loopback.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dsp.h"
#include "wavegen.h"

enum {
    ST_DELAY = 0,
    ST_FR,
    ST_THDN,
    ST_NOISE,
    ST_STEP,
    ST_DONE
};

#define LB_CAP_FRAMES 16384

void lb_default(LbConfig *c) {
    memset(c, 0, sizeof(*c));
    c->f1 = 20.0;
    c->f2 = 20000.0;
    c->points = 24;
    c->level_dbfs = -12.0;
    c->tol_db = 0.5;
    c->do_delay = 1;
    c->do_fr = 1;
    c->do_cal = 1;
    c->do_thdn = 1;
    c->do_noise = 1;
    c->do_step = 1;
}

const char *lb_step_name(int step) {
    switch (step) {
    case ST_DELAY: return "延迟";
    case ST_FR: return "频响";
    case ST_THDN: return "失真";
    case ST_NOISE: return "噪声";
    case ST_STEP: return "上升沿";
    default: return "完成";
    }
}

void lb_cancel(Loopback *lb) {
    lb->state = LB_IDLE;
    lb->stage[0] = 0;
}

static int ensure_cap(Loopback *lb, int frames) {
    if (frames <= lb->cap_alloc) return 0;
    float *p = (float *)realloc(lb->cap, (size_t)frames * sizeof(float));
    if (!p) return -1;
    lb->cap = p;
    lb->cap_alloc = frames;
    return 0;
}

int lb_begin(Loopback *lb, const LbConfig *cfg, double fs,
             const CalibData *saved_cal) {
    memset(lb, 0, sizeof(*lb));
    lb->cfg = *cfg;
    if (lb->cfg.f1 < 10.0) lb->cfg.f1 = 10.0;
    if (lb->cfg.f2 <= lb->cfg.f1) lb->cfg.f2 = lb->cfg.f1 * 10.0;
    if (lb->cfg.points < 4) lb->cfg.points = 4;
    if (lb->cfg.points > AN_MAX_FR) lb->cfg.points = AN_MAX_FR;
    if (lb->cfg.f2 > fs * 0.45) lb->cfg.f2 = fs * 0.45;
    lb->fs = fs > 0 ? fs : 48000.0;
    if (saved_cal) lb->cal_store = *saved_cal;
    if (ensure_cap(lb, LB_CAP_FRAMES) != 0) return -1;

    /* 对数间隔的频率点（确定性） */
    double lr = log(lb->cfg.f2 / lb->cfg.f1);
    for (int i = 0; i < lb->cfg.points; i++)
        lb->freqs[i] = lb->cfg.f1 *
                       exp(lr * (double)i / (double)(lb->cfg.points - 1));
    lb->nfreq = lb->cfg.points;
    lb->state = LB_RUN;
    lb->step = ST_DELAY;
    lb->phase = 0;
    lb->nfr = 0;
    lb->freq_idx = 0;
    lb->progress = 0;
    snprintf(lb->stage, sizeof(lb->stage), "准备");
    memset(&lb->res, 0, sizeof(lb->res));
    memset(&lb->rep, 0, sizeof(lb->rep));
    return 0;
}

static double amp_of(double dbfs) { return as_lin(dbfs); }

/* 设定激励并安排等待时间 */
static void set_tone(ASEngine *eng, GenCfg *gen, double f, double dbfs, int wave) {
    gen->wave = wave;
    gen->freq = f;
    gen->amp_db = dbfs;
    gen->enabled = 1;
    gen->ch_en[0] = 1;
    gen->ch_en[1] = 0;
    eng->gen_cfg = *gen;
}

static void set_silence(ASEngine *eng, GenCfg *gen) {
    gen->enabled = 0;
    eng->gen_cfg = *gen;
}

/* 采集一段输入，返回帧数 */
static int grab(ASEngine *eng, Loopback *lb, int frames) {
    if (ensure_cap(lb, frames) != 0) return 0;
    int64_t got = engine_read(eng, lb->cap, frames);
    lb->cap_n = (int)got;
    return lb->cap_n;
}

/* 用已知激励与采集做对齐（去掉未知的生成器相位） */
static double aligned_tone(const Loopback *lb, int n, double f, double *phase) {
    *phase = 0;
    int reflen = n / 2;
    if (reflen < 512) reflen = 512;
    if (reflen > n) reflen = n;
    float *ref = (float *)malloc((size_t)reflen * sizeof(float));
    if (!ref) return 0;
    for (int i = 0; i < reflen; i++)
        ref[i] = (float)sin(2.0 * AS_PI * f * (double)i / lb->fs);
    double peak = 0;
    double lag = an_xcorr_delay(ref, reflen, lb->cap, n, &peak);
    free(ref);
    int li = (int)(lag + 0.5);
    if (li < 0) li = 0;
    if (li + 1024 > n) li = n - 1024;
    if (li < 0) li = 0;
    int len = n - li;
    if (len > 8192) len = 8192;
    if (len < 512) len = 512;
    double ph = 0;
    double amp = an_tone(lb->cap + li, len, lb->fs, f, &ph);
    /* 对齐后参考相位为 0；残余延迟的相位修正 */
    double frac = lag - (double)li;
    double dph = -360.0 * f * frac / lb->fs;
    (void)dph;
    *phase = ph;
    return amp;
}

static int count_clip(const float *x, int n) {
    int c = 0;
    for (int i = 0; i < n; i++)
        if (x[i] >= 0.999f || x[i] <= -0.999f) c++;
    return c;
}

static void set_chirp(ASEngine *eng, GenCfg *gen, double dbfs) {
    gen->wave = W_CHIRP;
    gen->enabled = 1;
    gen->ch_en[0] = 1;
    gen->ch_en[1] = 0;
    gen->amp_db = dbfs;
    gen->offset = 0;
    gen->sweep_f1 = 300;
    gen->sweep_f2 = 6000;
    gen->sweep_sec = 0.25;
    pthread_mutex_lock(&eng->cfg_lock);
    eng->gen_cfg = *gen;
    gen_reset(&eng->gen);
    pthread_mutex_unlock(&eng->cfg_lock);
}

int lb_poll(Loopback *lb, ASEngine *eng, GenCfg *gen, double now) {
    if (lb->state != LB_RUN) return 0;
    if (now < lb->wait_until) {
        lb->progress = AS_MIN(0.985, lb->progress + 0.004);
        return 1;
    }

    switch (lb->step) {
    case ST_DELAY: {
        if (!lb->cfg.do_delay || lb->phase >= 2) {
            lb->step = ST_FR;
            lb->phase = 0;
            break;
        }
        if (lb->phase == 0) {
            set_chirp(eng, gen, lb->cfg.level_dbfs);
            snprintf(lb->stage, sizeof(lb->stage), "延迟测量：播扫频");
            lb->wait_until = now + 0.45;
            lb->phase = 1;
            break;
        }
        int n = grab(eng, lb, 16384);
        double out_start = (double)eng->gen.start_frame;
        int chirp_len = (int)(0.25 * lb->fs);
        float *ref = (float *)malloc((size_t)chirp_len * 2 * sizeof(float));
        if (n > chirp_len + 256 && ref) {
            Gen g2;
            GenCfg rc = *gen;
            gen_init(&g2, lb->fs);
            gen_render(&g2, &rc, ref, chirp_len);
            float *mono = (float *)malloc((size_t)chirp_len * sizeof(float));
            if (mono) {
                for (int i = 0; i < chirp_len; i++) mono[i] = ref[2 * i];
                double peak = 0;
                double lag = an_xcorr_delay(mono, chirp_len, lb->cap, n, &peak);
                double abs_first = (double)eng->ring_total - (double)n;
                double d = abs_first + lag - out_start;
                if (peak > 0.55 && d > 0.0 && d < 0.45 * lb->fs) {
                    lb->delay_ms = d / lb->fs * 1000.0;
                    lb->res.delay_ms = lb->delay_ms;
                    lb->res.have_delay = 1;
                }
                free(mono);
            }
        }
        free(ref);
        lb->phase = 2;
        break;
    }

    case ST_FR: {
        if (!lb->cfg.do_fr || lb->freq_idx >= lb->nfreq) {
            lb->step = ST_THDN;
            lb->phase = 0;
            break;
        }
        if (lb->phase == 0) {
            set_tone(eng, gen, lb->freqs[lb->freq_idx], lb->cfg.level_dbfs,
                     W_SINE);
            snprintf(lb->stage, sizeof(lb->stage), "频响 %d/%d：%.0f Hz",
                     lb->freq_idx + 1, lb->nfreq, lb->freqs[lb->freq_idx]);
            double settle = 40.0 / lb->freqs[lb->freq_idx];
            settle = AS_CLAMP(settle, 0.08, 0.30);
            lb->wait_until = now + settle + 0.20;
            lb->phase = 1;
            break;
        }
        int n = grab(eng, lb, 8192);
        double f = lb->freqs[lb->freq_idx];
        double amp_req = amp_of(lb->cfg.level_dbfs);
        if (n > 2048) {
            double ph = 0;
            double amp = aligned_tone(lb, n, f, &ph);
            if (amp > 0) {
                FrPoint *p = &lb->fr[lb->nfr];
                p->f = f;
                p->gain_raw_db = as_db(amp / amp_req);
                p->gain_db = p->gain_raw_db;
                p->phase_deg = ph;
                p->amp_err_db = as_db(amp) - lb->cfg.level_dbfs;
                if (lb->cfg.do_cal && lb->cal_store.valid) {
                    p->gain_db -= calib_gain_at(&lb->cal_store, f);
                    p->phase_deg -= calib_phase_at(&lb->cal_store, f);
                    while (p->phase_deg > 180.0) p->phase_deg -= 360.0;
                    while (p->phase_deg < -180.0) p->phase_deg += 360.0;
                }
                if (lb->nfr < AN_MAX_FR) lb->nfr++;
            }
            lb->res.clip_count += count_clip(lb->cap, n);
        }
        lb->freq_idx++;
        lb->phase = 0;
        lb->progress = 0.15 + 0.6 * (double)lb->freq_idx / (double)lb->nfreq;
        break;
    }


    case ST_THDN: {
        if (!lb->cfg.do_thdn) {
            lb->step = ST_NOISE;
            lb->phase = 0;
            break;
        }
        if (lb->phase == 0) {
            set_tone(eng, gen, 1000.0, lb->cfg.level_dbfs, W_SINE);
            snprintf(lb->stage, sizeof(lb->stage), "失真：1 kHz");
            lb->wait_until = now + 0.25;
            lb->phase = 1;
            break;
        }
        int n = grab(eng, lb, 16384);
        if (n >= 4096) {
            int nn = 4096;
            int start = n - nn;
            double *x = (double *)malloc((size_t)nn * sizeof(double));
            FFT fft;
            if (x && fft_init(&fft, nn) == 0) {
                for (int i = 0; i < nn; i++) x[i] = lb->cap[start + i];
                ToneMetrics tm;
                if (analyze_tone(&fft, x, nn, lb->fs, 1000.0, WIN_BH, &tm)) {
                    lb->res.thd_percent = tm.thd * 100.0;
                    lb->res.thdn_percent = tm.thdn * 100.0;
                    lb->res.snr_db = tm.snr;
                    lb->res.sinad_db = tm.sinad;
                    lb->res.have_thd = 1;
                }
                fft_free(&fft);
            }
            free(x);
            lb->res.clip_count += count_clip(lb->cap, n);
        }
        lb->phase = 0;
        lb->step = ST_NOISE;
        lb->progress = 0.82;
        break;
    }

    case ST_NOISE: {
        if (!lb->cfg.do_noise) {
            lb->step = ST_STEP;
            lb->phase = 0;
            break;
        }
        if (lb->phase == 0) {
            set_silence(eng, gen);
            snprintf(lb->stage, sizeof(lb->stage), "噪声底：静音采集");
            lb->wait_until = now + 0.30;
            lb->phase = 1;
            break;
        }
        int n = grab(eng, lb, 16384);
        if (n > 1024) {
            lb->res.noise_dbfs = an_noise_floor_dbfs(lb->cap, n);
            lb->res.have_noise = 1;
        }
        lb->phase = 0;
        lb->step = ST_STEP;
        lb->progress = 0.9;
        break;
    }

    case ST_STEP: {
        if (!lb->cfg.do_step) {
            lb->step = ST_DONE;
            break;
        }
        if (lb->phase == 0) {
            set_tone(eng, gen, 1000.0, lb->cfg.level_dbfs - 6.0, W_SQUARE);
            snprintf(lb->stage, sizeof(lb->stage), "上升沿：1 kHz 方波");
            lb->wait_until = now + 0.25;
            lb->phase = 1;
            break;
        }
        int n = grab(eng, lb, 8192);
        if (n > 1024) {
            lb->res.rise_us = an_rise_time_us(lb->cap, n, lb->fs);
            lb->res.have_step = 1;
        }
        lb->phase = 0;
        lb->step = ST_DONE;
        lb->progress = 0.96;
        break;
    }

    case ST_DONE: {
        set_silence(eng, gen);
        an_evaluate(lb->fr, lb->nfr, &lb->cal_store, &lb->res,
                    lb->cfg.tol_db);
        Report *r = &lb->rep;
        r->fs = lb->fs;
        r->level_dbfs = lb->cfg.level_dbfs;
        r->points = lb->nfreq;
        r->f1 = lb->cfg.f1;
        r->f2 = lb->cfg.f2;
        r->tol_db = lb->cfg.tol_db;
        r->use_calib = lb->cfg.do_cal && lb->cal_store.valid;
        r->res = lb->res;
        r->nfr = lb->nfr;
        for (int i = 0; i < lb->nfr; i++) r->fr[i] = lb->fr[i];
        report_build(r, &lb->cal_store);
        lb->state = LB_DONE;
        lb->progress = 1.0;
        snprintf(lb->stage, sizeof(lb->stage), "完成");
        return 0;
    }
    default:
        break;
    }
    return 1;
}

