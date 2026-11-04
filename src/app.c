#include "app.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const int FFT_SIZES[8] = {1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072};
static const int WF_SIZES[5] = {512, 1024, 2048, 4096, 8192};
static const int RATES[6] = {44100, 48000, 88200, 96000, 176400, 192000};

static const char *WAVE_NAMES[W_COUNT] = {
    "正弦波", "方波", "三角波", "锯齿波", "白噪声", "粉红噪声", "双音", "对数扫频",
    "静音"};
static const char *WIN_ITEMS[WIN_COUNT] = {
    "Blackman-Harris", "Hann", "Hamming", "Blackman", "Flat-top", "Rect"};
static const char *CMAP_ITEMS[4] = {"鼠尾草", "石墨", "冷灰", "靛青"};
static const char *ROUTE_ITEMS[3] = {"信号发生器", "音频文件", "混合"};
static const char *COUPLING_ITEMS[2] = {"AC (去直流)", "DC (原始)"};
static const char *MATH_ITEMS[3] = {"关闭", "A + B", "A - B"};
static const char *EDGE_ITEMS[2] = {"上升沿", "下降沿"};
static const char *SRC_ITEMS[3] = {"CH1", "CH2", "自由运行"};
static const char *TAB_ITEMS[TAB_N] = {"波形 / 时域", "频谱 / FFT", "频率响应", "闭环测试"};

const char *as_wave_name(int w) {
    return (w >= 0 && w < W_COUNT) ? WAVE_NAMES[w] : "?";
}
const char *as_win_name(int w) {
    return (w >= 0 && w < WIN_COUNT) ? WIN_ITEMS[w] : "?";
}
const char *as_cmap_name(int c) {
    return (c >= 0 && c < 4) ? CMAP_ITEMS[c] : "?";
}
const char *as_route_name(int r) {
    return (r >= 0 && r < 3) ? ROUTE_ITEMS[r] : "?";
}
const char **as_tab_items(void) { return TAB_ITEMS; }
const char **as_src_items(void) { return SRC_ITEMS; }
const char **as_edge_items(void) { return EDGE_ITEMS; }
const char **as_coupling_items(void) { return COUPLING_ITEMS; }
const char **as_math_items(void) { return MATH_ITEMS; }
const char **as_win_items(void) { return WIN_ITEMS; }
const char **as_cmap_items(void) { return CMAP_ITEMS; }
const char **as_route_items(void) { return ROUTE_ITEMS; }
int as_rate(int idx) { return RATES[AS_CLAMP(idx, 0, 5)]; }
int as_rate_count(void) { return 6; }
int as_fft_size(int idx) { return FFT_SIZES[AS_CLAMP(idx, 0, 7)]; }
int as_wf_size(int idx) { return WF_SIZES[AS_CLAMP(idx, 0, 4)]; }
int as_tab_count(void) { return TAB_N; }

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void app_set_message(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof(a->msg), fmt, ap);
    va_end(ap);
    a->msg_time = now_sec();
}

/* ---- defaults / settings ---------------------------------------------- */
static void set_defaults(App *a) {
    a->theme = THEME_DARK;
    {
        const char *home = getenv("HOME");
        snprintf(a->out_dir, sizeof(a->out_dir), "%s/Documents/AudioScope",
                 home ? home : ".");
        mkdir(a->out_dir, 0755);
    }
    lb_default(&a->lbcfg);
    memset(a->gen.ch_en, 0, sizeof(a->gen.ch_en));
    a->gen.wave = W_SINE;
    a->gen.freq = 1000.0;
    a->gen.amp_db = -12.0;
    a->gen.enabled = 1;
    a->gen.ch_en[0] = 1;
    a->gen.sweep_f1 = 20.0;
    a->gen.sweep_f2 = 20000.0;
    a->gen.sweep_sec = 2.0;
    a->gen.dual_freq = 1300.0;
    a->route = 0;
    a->muted = 0;
    a->volts_at_fs = 1.0;
    a->ch_gain[0] = a->ch_gain[1] = 1.0;
    a->gen_volts = 1.0;
    a->trig_src = 0;
    a->trig_edge = 0;
    a->trig_level = 0.0;
    a->trig_hyst = 0.02;
    a->time_div = 1e-3;
    a->volt_div = 0.2;
    a->v_off = 0.0;
    a->coupling = 0;
    a->math = 0;
    a->show_ch[0] = a->show_ch[1] = 1;
    a->average = 1;
    a->fft_size_idx = 4;
    a->fft_win = WIN_BH;
    a->peak_hold = 1;
    a->fft_avg = 0.6;
    a->wf_fft_idx = 2;
    a->wf_cmap = 0;
    a->wf_span = 40.0;
    a->wf_low = -110.0;
    a->wf_high = -10.0;
    a->spec_low = -140.0;
    a->spec_high = 0.0;
    a->bode_f1 = 20.0;
    a->bode_f2 = 20000.0;
    a->bode_points = 40;
    a->bode_ref = 0;
    a->bode_dut = 1;
    a->file_gain_db = -6.0;
    a->rate_idx = 1;
    a->req_rate = 48000.0;
    a->tab = TAB_SCOPE;
    a->last_dir[0] = 0;
}

static void config_path(char *buf, size_t n) {
    const char *home = getenv("HOME");
    snprintf(buf, n, "%s/Library/Application Support/AudioScope", home ? home : ".");
    mkdir(buf, 0755);
    snprintf(buf, n, "%s/Library/Application Support/AudioScope/config.txt",
             home ? home : ".");
}

void app_save_settings(App *a) {
    char path[512];
    config_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "gen.wave=%d\ngen.freq=%.6f\ngen.amp=%.4f\ngen.enabled=%d\n",
            a->gen.wave, a->gen.freq, a->gen.amp_db, a->gen.enabled);
    fprintf(f, "gen.ch0=%d\ngen.ch1=%d\ngen.ph0=%.3f\ngen.ph1=%.3f\n"
               "gen.inv0=%d\ngen.inv1=%d\n",
            a->gen.ch_en[0], a->gen.ch_en[1], a->gen.ch_phase[0],
            a->gen.ch_phase[1], a->gen.ch_inv[0], a->gen.ch_inv[1]);
    fprintf(f, "gen.dual=%.3f\ngen.sf1=%.3f\ngen.sf2=%.3f\ngen.ss=%.3f\n",
            a->gen.dual_freq, a->gen.sweep_f1, a->gen.sweep_f2, a->gen.sweep_sec);
    fprintf(f, "route=%d\nmuted=%d\nin_dev=%d\nout_dev=%d\nrate_idx=%d\n",
            a->route, a->muted, a->in_dev, a->out_dev, a->rate_idx);
    fprintf(f, "cal.volts=%.6f\ncal.g0=%.6f\ncal.g1=%.6f\ncal.gv=%.6f\n",
            a->volts_at_fs, a->ch_gain[0], a->ch_gain[1], a->gen_volts);
    fprintf(f, "scope.src=%d\nscope.edge=%d\nscope.level=%.6f\nscope.hyst=%.6f\n",
            a->trig_src, a->trig_edge, a->trig_level, a->trig_hyst);
    fprintf(f, "scope.tdiv=%.9f\nscope.vdiv=%.9f\nscope.off=%.6f\n",
            a->time_div, a->volt_div, a->v_off);
    fprintf(f, "scope.coupling=%d\nscope.math=%d\nscope.s0=%d\nscope.s1=%d\n"
               "scope.avg=%d\n",
            a->coupling, a->math, a->show_ch[0], a->show_ch[1], a->average);
    fprintf(f, "fft.size=%d\nfft.win=%d\nfft.avg=%.4f\nfft.peak=%d\n",
            a->fft_size_idx, a->fft_win, a->fft_avg, a->peak_hold);
    fprintf(f, "wf.size=%d\nwf.cmap=%d\nwf.span=%.2f\nwf.low=%.2f\nwf.high=%.2f\n",
            a->wf_fft_idx, a->wf_cmap, a->wf_span, a->wf_low, a->wf_high);
    fprintf(f, "bode.f1=%.4f\nbode.f2=%.4f\nbode.n=%d\nbode.ref=%d\nbode.dut=%d\n",
            a->bode_f1, a->bode_f2, a->bode_points, a->bode_ref, a->bode_dut);
    fprintf(f, "file.gain=%.3f\nfile.dir=%s\n", a->file_gain_db, a->last_dir);
    fclose(f);
}

static void set_int(int *dst, const char *val) { *dst = atoi(val); }
static void set_dbl(double *dst, const char *val) { *dst = atof(val); }

void app_load_settings(App *a) {
    char path[512];
    config_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[600];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line;
        char *val = eq + 1;
        size_t vl = strlen(val);
        while (vl > 0 && (val[vl - 1] == '\n' || val[vl - 1] == '\r')) val[--vl] = 0;
        if (!strcmp(key, "gen.wave")) set_int(&a->gen.wave, val);
        else if (!strcmp(key, "gen.freq")) set_dbl(&a->gen.freq, val);
        else if (!strcmp(key, "gen.amp")) set_dbl(&a->gen.amp_db, val);
        else if (!strcmp(key, "gen.enabled")) set_int(&a->gen.enabled, val);
        else if (!strcmp(key, "gen.ch0")) set_int(&a->gen.ch_en[0], val);
        else if (!strcmp(key, "gen.ch1")) set_int(&a->gen.ch_en[1], val);
        else if (!strcmp(key, "gen.ph0")) set_dbl(&a->gen.ch_phase[0], val);
        else if (!strcmp(key, "gen.ph1")) set_dbl(&a->gen.ch_phase[1], val);
        else if (!strcmp(key, "gen.inv0")) set_int(&a->gen.ch_inv[0], val);
        else if (!strcmp(key, "gen.inv1")) set_int(&a->gen.ch_inv[1], val);
        else if (!strcmp(key, "gen.dual")) set_dbl(&a->gen.dual_freq, val);
        else if (!strcmp(key, "gen.sf1")) set_dbl(&a->gen.sweep_f1, val);
        else if (!strcmp(key, "gen.sf2")) set_dbl(&a->gen.sweep_f2, val);
        else if (!strcmp(key, "gen.ss")) set_dbl(&a->gen.sweep_sec, val);
        else if (!strcmp(key, "route")) set_int(&a->route, val);
        else if (!strcmp(key, "muted")) set_int(&a->muted, val);
        else if (!strcmp(key, "in_dev")) set_int(&a->in_dev, val);
        else if (!strcmp(key, "out_dev")) set_int(&a->out_dev, val);
        else if (!strcmp(key, "rate_idx")) set_int(&a->rate_idx, val);
        else if (!strcmp(key, "cal.volts")) set_dbl(&a->volts_at_fs, val);
        else if (!strcmp(key, "cal.g0")) set_dbl(&a->ch_gain[0], val);
        else if (!strcmp(key, "cal.g1")) set_dbl(&a->ch_gain[1], val);
        else if (!strcmp(key, "cal.gv")) set_dbl(&a->gen_volts, val);
        else if (!strcmp(key, "scope.src")) set_int(&a->trig_src, val);
        else if (!strcmp(key, "scope.edge")) set_int(&a->trig_edge, val);
        else if (!strcmp(key, "scope.level")) set_dbl(&a->trig_level, val);
        else if (!strcmp(key, "scope.hyst")) set_dbl(&a->trig_hyst, val);
        else if (!strcmp(key, "scope.tdiv")) set_dbl(&a->time_div, val);
        else if (!strcmp(key, "scope.vdiv")) set_dbl(&a->volt_div, val);
        else if (!strcmp(key, "scope.off")) set_dbl(&a->v_off, val);
        else if (!strcmp(key, "scope.coupling")) set_int(&a->coupling, val);
        else if (!strcmp(key, "scope.math")) set_int(&a->math, val);
        else if (!strcmp(key, "scope.s0")) set_int(&a->show_ch[0], val);
        else if (!strcmp(key, "scope.s1")) set_int(&a->show_ch[1], val);
        else if (!strcmp(key, "scope.avg")) set_int(&a->average, val);
        else if (!strcmp(key, "fft.size")) set_int(&a->fft_size_idx, val);
        else if (!strcmp(key, "fft.win")) set_int(&a->fft_win, val);
        else if (!strcmp(key, "fft.avg")) set_dbl(&a->fft_avg, val);
        else if (!strcmp(key, "fft.peak")) set_int(&a->peak_hold, val);
        else if (!strcmp(key, "wf.size")) set_int(&a->wf_fft_idx, val);
        else if (!strcmp(key, "wf.cmap")) set_int(&a->wf_cmap, val);
        else if (!strcmp(key, "wf.span")) set_dbl(&a->wf_span, val);
        else if (!strcmp(key, "wf.low")) set_dbl(&a->wf_low, val);
        else if (!strcmp(key, "wf.high")) set_dbl(&a->wf_high, val);
        else if (!strcmp(key, "bode.f1")) set_dbl(&a->bode_f1, val);
        else if (!strcmp(key, "bode.f2")) set_dbl(&a->bode_f2, val);
        else if (!strcmp(key, "bode.n")) set_int(&a->bode_points, val);
        else if (!strcmp(key, "bode.ref")) set_int(&a->bode_ref, val);
        else if (!strcmp(key, "bode.dut")) set_int(&a->bode_dut, val);
        else if (!strcmp(key, "file.gain")) set_dbl(&a->file_gain_db, val);
        else if (!strcmp(key, "file.dir"))
            snprintf(a->last_dir, sizeof(a->last_dir), "%s", val);
    }
    fclose(f);
    a->fft_size_idx = AS_CLAMP(a->fft_size_idx, 0, 7);
    a->wf_fft_idx = AS_CLAMP(a->wf_fft_idx, 0, 4);
    a->rate_idx = AS_CLAMP(a->rate_idx, 0, 5);
    a->fft_win = AS_CLAMP(a->fft_win, 0, WIN_COUNT - 1);
    a->tab = AS_CLAMP(a->tab, 0, TAB_N - 1);
    a->gen.wave = AS_CLAMP(a->gen.wave, 0, W_COUNT - 1);
    a->req_rate = (double)as_rate(a->rate_idx);
}



/* ---- buffers ---------------------------------------------------------- */
static void app_ensure_fft(App *a) {
    int n = as_fft_size(a->fft_size_idx);
    if (a->fft.n != n) {
        fft_free(&a->fft);
        fft_init(&a->fft, n);
        free(a->dbuf);
        free(a->win);
        free(a->spec_amp);
        free(a->spec_freq);
        a->dbuf = (double *)malloc(sizeof(double) * (size_t)n);
        a->win = (double *)malloc(sizeof(double) * (size_t)n);
        a->spec_amp = (double *)malloc(sizeof(double) * (size_t)(n / 2 + 2));
        a->spec_freq = (double *)malloc(sizeof(double) * (size_t)(n / 2 + 2));
    }
    win_fill((WinKind)a->fft_win, a->win, n);
}

void app_ensure_fft_pub(App *a) { app_ensure_fft(a); }


static void app_ensure_wfft(App *a) {
    int n = as_wf_size(a->wf_fft_idx);
    if (a->wfft.n != n) {
        fft_free(&a->wfft);
        fft_init(&a->wfft, n);
    }
}

static void app_alloc_capture(App *a) {
    int max_win = (int)(a->fs * 2.0) + 8;
    int need = max_win + max_win / 2 + 64;
    if (need > a->cap_frames) {
        free(a->cap);
        free(a->mono1);
        free(a->mono2);
        free(a->fftbuf);
        a->cap = (float *)malloc(sizeof(float) * (size_t)need * 2);
        a->mono1 = (float *)malloc(sizeof(float) * (size_t)need);
        a->mono2 = (float *)malloc(sizeof(float) * (size_t)need);
        a->fftbuf = (float *)malloc(sizeof(float) * 131072 * 2);
        a->cap_frames = need;
    }
}

void app_init(App *a) {
    memset(a, 0, sizeof(*a));
    set_defaults(a);
    pthread_mutex_init(&a->eng.lock, NULL);
    pthread_mutex_init(&a->eng.cfg_lock, NULL);
    app_load_settings(a);
    a->ndev = as_devices(a->devs, 48);
    int found_in = 0, found_out = 0;
    for (int i = 0; i < a->ndev; i++) {
        if ((int)a->devs[i].id == a->in_dev && a->devs[i].is_input) found_in = 1;
        if ((int)a->devs[i].id == a->out_dev && a->devs[i].is_output) found_out = 1;
    }
    if (!found_in) a->in_dev = as_default_device(0);
    if (!found_out) a->out_dev = as_default_device(1);
    a->fs = (double)as_rate(a->rate_idx);

    a->wf.w = WF_COLS;
    a->wf.h = WF_ROWS;
    a->wf.rgba = (unsigned char *)calloc((size_t)WF_COLS * WF_ROWS * 4, 1);
    a->wf.row = 0;
    a->wf.count = 0;
    a->spec_lin = (float *)malloc(sizeof(float) * SPEC_COLS * 2);
    a->spec_peak = (float *)malloc(sizeof(float) * SPEC_COLS);
    a->logtmp = (double *)malloc(sizeof(double) * SPEC_COLS);
    a->wf_win = (double *)malloc(sizeof(double) * 8192);
    win_fill(WIN_HANN, a->wf_win, 8192);
    for (int j = 0; j < SPEC_COLS; j++) {
        a->spec_lin[j] = -1.0f;
        a->spec_lin[SPEC_COLS + j] = -1.0f;
        a->spec_peak[j] = -200.0f;   /* 峰值保持初值必须是极小值，否则永远不会上升 */
        a->spec.db[0][j] = a->spec.db[1][j] = a->spec.db[2][j] = -180.0f;
    }
    memset(&a->ui, 0, sizeof(a->ui));
    memset(&a->ui_src, 0, sizeof(a->ui_src));
    app_ensure_fft(a);
    app_ensure_wfft(a);
    a->eng.gen_cfg = a->gen;
    a->eng.route = a->route;
    a->eng.muted = a->muted;
    a->eng.file_gain = (float)as_lin(a->file_gain_db);
    app_alloc_capture(a);
    theme_set(a->theme);
    {
        char cp[800];
        snprintf(cp, sizeof(cp), "%s/calibration.json", a->out_dir);
        calib_load(&a->cal, cp);
    }
    app_set_message(a, "就绪：点「开始采集」或按 AUTO (F6)");
}

void app_sync_gen(App *a) {
    /* 生成器配置只在锁内替换，避免音频线程读到写了一半的结构 */
    pthread_mutex_lock(&a->eng.cfg_lock);
    a->eng.gen_cfg = a->gen;
    pthread_mutex_unlock(&a->eng.cfg_lock);
    a->eng.route = a->route;
    a->eng.muted = a->muted;
    a->eng.file_gain = (float)as_lin(a->file_gain_db);
}

int app_theme_toggle(App *a) {
    theme_toggle();
    a->theme = g_theme_mode;
    return a->theme;
}

/* ---- 1.1 闭环测试 ----------------------------------------------------- */
void app_lb_start(App *a, int calibrate) {
    if (!a->running) {
        app_set_message(a, "请先开始采集，再做闭环测试");
        return;
    }
    if (calibrate) {
        a->lbcfg.do_cal = 0;      /* 校准本身不做扣除 */
        a->lbcfg.do_delay = 1;
        a->lbcfg.do_fr = 1;
        a->lbcfg.do_thdn = 0;
        a->lbcfg.do_noise = 0;
        a->lbcfg.do_step = 0;
    } else {
        lb_default(&a->lbcfg);

    }
    a->lbcfg.level_dbfs = a->gen.amp_db;
    if (lb_begin(&a->lb, &a->lbcfg, a->fs, &a->cal) != 0) {
        app_set_message(a, "无法分配采集缓冲");
        return;
    }
    a->lb.cal_store = a->cal;
    app_set_message(a, calibrate ? "校准开始：请把输出直连到输入"
                                 : "闭环测试开始");
}

void app_lb_write_report(App *a) {
    Loopback *lb = &a->lb;
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);

    Report *r = &lb->rep;
    /* 设备名与设置补齐 */
    for (int i = 0; i < a->ndev; i++) {
        if ((int)a->devs[i].id == a->in_dev)
            snprintf(r->in_dev, sizeof(r->in_dev), "%s", a->devs[i].name);
        if ((int)a->devs[i].id == a->out_dev)
            snprintf(r->out_dev, sizeof(r->out_dev), "%s", a->devs[i].name);
    }
    r->channels = 1;
    r->block = a->eng.block;
    uint32_t h = 2166136261u;
    const char *key = "audioscope-lb";
    for (const char *p = key; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
    h = (h ^ (uint32_t)r->fs) * 16777619u;
    h = (h ^ (uint32_t)(r->level_dbfs * 100)) * 16777619u;
    h = (h ^ (uint32_t)r->points) * 16777619u;
    h = (h ^ (uint32_t)(r->f1)) * 16777619u;
    h = (h ^ (uint32_t)(r->f2)) * 16777619u;
    r->settings_hash = h;

    char path[800];
    snprintf(path, sizeof(path), "%s/report-%s.json", a->out_dir, stamp);
    if (report_json(r, path) == 0)
        snprintf(a->last_dir, sizeof(a->last_dir), "%s", path);
    snprintf(path, sizeof(path), "%s/report-%s.csv", a->out_dir, stamp);
    report_csv(r, path);
    if (lb->cap && lb->cap_n > 0) {
        snprintf(path, sizeof(path), "%s/capture-%s.wav", a->out_dir, stamp);
        report_wav24(lb->cap, lb->cap_n, a->fs, path);
    }

    if (a->lbcfg.do_cal == 0) {
        /* 校准模式：把这次直连结果存成校准文件 */
        CalibData c;
        calib_clear(&c);
        c.valid = lb->nfr > 2;
        c.fs = a->fs;
        c.n = lb->nfr < CAL_MAX ? lb->nfr : CAL_MAX;
        for (int i = 0; i < c.n; i++) {
            c.f[i] = lb->fr[i].f;
            c.gain_db[i] = lb->fr[i].gain_raw_db;
            c.phase_deg[i] = lb->fr[i].phase_deg;
        }
        c.delay_ms = lb->delay_ms;
        c.amp_err_db = 0;
        a->cal = c;
        char cp[800];
        snprintf(cp, sizeof(cp), "%s/calibration.json", a->out_dir);
        calib_save(&c, cp);
        app_set_message(a, "校准完成：%d 点已保存到 %s", c.n, cp);
    } else {
        app_set_message(a, "报告已保存：%s", a->out_dir);
    }
}

void app_stop(App *a) {
    if (!a->running) return;
    engine_stop(&a->eng);
    a->running = 0;
    app_set_message(a, "已停止");
}

void app_start(App *a) {
    if (a->running) return;
    double rate = (double)as_rate(a->rate_idx);
    int rc = engine_start(&a->eng, (AudioDeviceID)a->in_dev,
                          (AudioDeviceID)a->out_dev, rate, 512);
    if (rc != 0) {
        app_set_message(a, "无法打开音频设备（错误 %d），请检查设备与权限", rc);
        return;
    }
    a->fs = a->eng.fs;
    a->running = 1;
    a->eng.gen_cfg = a->gen;
    a->eng.route = a->route;
    a->eng.muted = a->muted;
    a->frozen = 0;
    app_alloc_capture(a);
    app_set_message(a, "运行中 @ %.0f Hz", a->fs);
}

void app_apply_devices(App *a, int in_id, int out_id) {
    int was = a->running;
    if (was) app_stop(a);
    a->in_dev = in_id;
    a->out_dev = out_id;
    if (was) app_start(a);
}

void app_load_file(App *a, const char *path) {
    int rc = file_load(&a->eng.file, path, a->fs);
    if (rc != 0) {
        app_set_message(a, "无法读取音频文件（%d）", rc);
        return;
    }
    a->eng.file_pos = 0;
    a->eng.playing = 1;
    a->eng.loop = 0;
    a->eng.finished = 0;
    if (a->route == 0) {
        a->route = 1;
        a->eng.route = 1;
    }
    const char *base = strrchr(path, '/');
    int dirlen = base ? (int)(base - path) : 0;
    if (dirlen > 0 && dirlen < (int)sizeof(a->last_dir))
        snprintf(a->last_dir, sizeof(a->last_dir), "%.*s", dirlen, path);
    app_set_message(a, "播放: %s (%.1f s)", a->eng.file.name,
                    a->eng.file.duration);
}

/* ---- colour maps for the waterfall ------------------------------------ */
/* 瀑布配色：单调整体亮度上升，不用彩虹色（色盲安全、且不抢主曲线） */
void as_cmap(int palette, float x, unsigned char out[4]) {
    static const float stops[5] = {0.00f, 0.28f, 0.55f, 0.80f, 1.00f};
    static const float cr[4][5] = {
        {0x1b, 0x2f, 0x5c, 0x9d, 0xd8},
        {0x14, 0x3a, 0x76, 0xb6, 0xe4},
        {0x1a, 0x43, 0x8a, 0xc9, 0xef},
        {0x10, 0x2c, 0x4e, 0x84, 0xbe}};
    static const float cg[4][5] = {
        {0x20, 0x44, 0x7f, 0xb3, 0xcf},
        {0x18, 0x42, 0x7f, 0xbc, 0xe6},
        {0x1d, 0x30, 0x5a, 0x8f, 0xd9},
        {0x14, 0x38, 0x60, 0x93, 0xc8}};
    static const float cb[4][5] = {
        {0x22, 0x40, 0x72, 0xa4, 0xae},
        {0x1a, 0x42, 0x7c, 0xb7, 0xe0},
        {0x1e, 0x2a, 0x45, 0x68, 0xbb},
        {0x12, 0x34, 0x56, 0x84, 0xb0}};
    int pal = AS_CLAMP(palette, 0, 3);
    if (x < 0) x = 0;
    if (x > 1) x = 1;
    int i = 0;
    while (i < 3 && x > stops[i + 1]) i++;
    float span = stops[i + 1] - stops[i];
    float t = span > 1e-6f ? (x - stops[i]) / span : 0;
    out[0] = (unsigned char)(cr[pal][i] + (cr[pal][i + 1] - cr[pal][i]) * t);
    out[1] = (unsigned char)(cg[pal][i] + (cg[pal][i + 1] - cg[pal][i]) * t);
    out[2] = (unsigned char)(cb[pal][i] + (cb[pal][i + 1] - cb[pal][i]) * t);
    out[3] = 255;
}

static void cmap_lookup(int palette, float x, unsigned char *out) {
    as_cmap(palette, x, out);
}

static void waterfall_push(App *a, int avail) {
    int wfn = as_wf_size(a->wf_fft_idx);
    if (wfn > avail) wfn = avail;
    if (wfn < 256) return;
    double fs = a->fs;
    int half = wfn / 2;
    for (int i = 0; i < wfn; i++)
        a->dbuf[i] = a->fftbuf[2 * (avail - wfn + i)];
    if (a->wfft.n != wfn) {
        fft_free(&a->wfft);
        fft_init(&a->wfft, wfn);
    }
    win_fill(WIN_HANN, a->wf_win, wfn);
    fft_spectrum(&a->wfft, a->dbuf, a->wf_win, wfn, a->spec_amp, half);
    for (int k = 0; k <= half; k++) a->spec_freq[k] = (double)k * fs / wfn;
    log_bins(a->spec_freq, a->spec_amp, half + 1, WF_COLS, 20.0, fs * 0.5,
             a->logtmp);
    unsigned char *row = a->wf.rgba + (size_t)a->wf.row * WF_COLS * 4;
    double lo = a->wf_low, hi = a->wf_high;
    if (hi <= lo) hi = lo + 1.0;
    for (int j = 0; j < WF_COLS; j++) {
        double db = as_db(a->logtmp[j]);
        float t = (float)((db - lo) / (hi - lo));
        cmap_lookup(a->wf_cmap, t, row + j * 4);
    }
    a->wf.row = (a->wf.row + 1) % WF_ROWS;
    if (a->wf.count < WF_ROWS) a->wf.count++;
}

/* ---- spectrum / tone analysis ----------------------------------------- */
static void app_analyse(App *a, int avail, int n) {
    double fs = a->fs;
    int half = n / 2;
    double avg = AS_CLAMP(a->fft_avg, 0.0, 0.95);
    for (int k = 0; k <= half; k++) a->spec_freq[k] = (double)k * fs / n;
    double hint = 0.0;
    if (a->gen.enabled && (a->gen.wave == W_SINE || a->gen.wave == W_SQUARE ||
                           a->gen.wave == W_TRI || a->gen.wave == W_SAW))
        hint = a->gen.freq;

    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < n; i++)
            a->dbuf[i] = a->fftbuf[2 * (avail - n + i) + c];
        fft_spectrum(&a->fft, a->dbuf, a->win, n, a->spec_amp, half);
        log_bins(a->spec_freq, a->spec_amp, half + 1, SPEC_COLS, 20.0, fs * 0.5,
                 a->logtmp);
        for (int j = 0; j < SPEC_COLS; j++) {
            float v = (float)a->logtmp[j];
            float old = a->spec_lin[c * SPEC_COLS + j];
            float lin = (old < 0.0f) ? v
                                     : (float)(avg * old + (1.0 - avg) * v);
            a->spec_lin[c * SPEC_COLS + j] = lin;
            if (v > a->spec_peak[j]) a->spec_peak[j] = v;
            a->spec.db[c][j] = (float)as_db(lin);
        }
        ToneMetrics tm;
        if (analyze_tone(&a->fft, a->dbuf, n, fs, hint, (WinKind)a->fft_win, &tm)) {
            a->tone[c] = tm;
            a->tone_valid[c] = 1;
        } else {
            a->tone_valid[c] = 0;
        }
    }
    for (int j = 0; j < SPEC_COLS; j++)
        a->spec.db[2][j] = a->peak_hold ? (float)as_db(a->spec_peak[j])
                                       : a->spec.db[0][j];

    /* harmonic markers (channel 1) */
    a->spec.mark_n = 0;
    if (a->tone_valid[0] && a->tone[0].freq > 0) {
        double f0 = a->tone[0].freq;
        a->spec.marks_f[0] = f0;
        a->spec.marks_db[0] = (float)a->tone[0].amp_db;
        a->spec.mark_n = 1;
        for (int k = 0; k < 10 && a->spec.mark_n < 11; k++) {
            if (a->tone[0].harm[k] <= 0) continue;
            a->spec.marks_f[a->spec.mark_n] = f0 * (double)(k + 2);
            a->spec.marks_db[a->spec.mark_n] = (float)as_db(a->tone[0].harm[k]);
            a->spec.mark_n++;
        }
    }
    waterfall_push(a, avail);
}


/* ---- main update ------------------------------------------------------ */
void app_tick(App *a) {
    a->now = now_sec();
    if (a->last_fft == 0.0) a->last_fft = a->now - 1.0;
    if (!a->running || a->frozen) return;

    /* 闭环测试优先占用采集，避免和示波器抢 ring 数据 */
    if (a->lb.state == LB_RUN) {
        lb_poll(&a->lb, &a->eng, &a->gen, a->now);
        if (a->lb.state == LB_DONE) {
            app_lb_write_report(a);
            a->gen.enabled = 1;
            app_sync_gen(a);
        }
        return;
    }
    double fs = a->fs;
    if (!a->mono1 || !a->cap) return;

    /* ---------- scope capture ---------- */
    double win_s = a->time_div * 10.0;
    if (win_s < 5.0 / fs) win_s = 5.0 / fs;
    int win = (int)(win_s * fs);
    if (win < 64) win = 64;
    if (win > (int)(fs * 2.0)) win = (int)(fs * 2.0);
    int extra = win / 2;
    if (extra < 1024) extra = 1024;
    int need = win + extra;
    if (need > a->cap_frames) need = a->cap_frames;
    int64_t got = engine_read(&a->eng, a->cap, need);
    if (got < (int64_t)win + 8) return;
    int total = (int)got;

    if (a->coupling == 0) {
        double m0 = 0, m1 = 0;
        for (int i = 0; i < total; i++) {
            m0 += a->cap[2 * i];
            m1 += a->cap[2 * i + 1];
        }
        m0 /= (double)total;
        m1 /= (double)total;
        for (int i = 0; i < total; i++) {
            a->cap[2 * i] = (float)(a->cap[2 * i] - m0);
            a->cap[2 * i + 1] = (float)(a->cap[2 * i + 1] - m1);
        }
    }
    for (int i = 0; i < total; i++) {
        a->mono1[i] = a->cap[2 * i];
        a->mono2[i] = a->cap[2 * i + 1];
    }

    int limit = total - win;
    double start = (double)limit;
    int src = AS_CLAMP(a->trig_src, 0, 1);
    if (a->trig_src >= 0) {
        const float *tr = (a->trig_src == 0) ? a->mono1 : a->mono2;
        double pos = find_trigger(tr, limit + 1, a->trig_level,
                                  a->trig_edge, a->trig_hyst, 0);
        if (pos >= 0) start = pos;
    }
    int s0 = (int)(start + 0.5);
    if (s0 < 0) s0 = 0;
    if (s0 > limit) s0 = limit;

    double sc0 = a->volts_at_fs * a->ch_gain[0];
    double sc1 = a->volts_at_fs * a->ch_gain[1];
    time_stats(a->mono1 + s0, win, fs, sc0, &a->tstat[0]);
    time_stats(a->mono2 + s0, win, fs, sc1, &a->tstat[1]);

    int disp = win < SCOPE_MAX ? win : SCOPE_MAX;
    int stride = win / disp;
    if (stride < 1) stride = 1;
    for (int i = 0; i < disp; i++) {
        int k = s0 + i * stride;
        if (k >= total) k = total - 1;
        float v1 = a->cap[2 * k] * (float)sc0;
        float v2 = a->cap[2 * k + 1] * (float)sc1;
        a->scope.ch1[i] = v1;
        a->scope.ch2[i] = v2;
        if (a->math == 1) a->scope.math[i] = v1 + v2;
        else if (a->math == 2) a->scope.math[i] = v1 - v2;
    }
    a->scope.n = disp;
    a->scope.span = (double)win / fs;
    a->scope.dt = (double)stride / fs;
    a->scope.has_math = a->math != 0;
    a->scope.has_ref = 0;
    (void)src;

    /* ---------- spectrum ---------- */
    if (a->now - a->last_fft >= 0.09) {
        a->last_fft = a->now;
        int n = as_fft_size(a->fft_size_idx);
        if (n > a->fft.n) n = a->fft.n;
        int64_t g2 = engine_read(&a->eng, a->fftbuf, n);
        if (g2 >= (int64_t)n) app_analyse(a, (int)g2, n);
    }

    /* ---------- bode sweep ---------- */
    if (a->bode_active && a->now >= a->bode_next) {
        int n = (int)(fs * 0.15);
        if (n > a->cap_frames) n = a->cap_frames;
        int64_t g3 = engine_read(&a->eng, a->cap, n);
        if (g3 > 1024) {
            int cnt = (int)g3;
            double f = (a->bode_idx < a->bode_points)
                           ? a->bode_f1 *
                                 pow(a->bode_f2 / a->bode_f1,
                                     (double)a->bode_idx /
                                         (double)(a->bode_points - 1))
                           : a->bode_f1;
            double ref_r = 0, ref_i = 0, dut_r = 0, dut_i = 0;
            for (int i = 0; i < cnt; i++) {
                a->mono1[i] = a->cap[2 * i];
                a->mono2[i] = a->cap[2 * i + 1];
            }
            dft_complex_at(a->mono1, cnt, f, fs, &ref_r, &ref_i);
            dft_complex_at(a->mono2, cnt, f, fs, &dut_r, &dut_i);
            double rm = sqrt(ref_r * ref_r + ref_i * ref_i);
            double dm = sqrt(dut_r * dut_r + dut_i * dut_i);
            if (rm > 1e-9 && dm > 1e-12 && a->bode_idx < BODE_MAX) {
                a->bode.f[a->bode_idx] = f;
                a->bode.g[a->bode_idx] =
                    20.0 * log10(dm / rm);
                double ph = atan2(dut_i, dut_r) - atan2(ref_i, ref_r);
                while (ph > M_PI) ph -= 2 * M_PI;
                while (ph < -M_PI) ph += 2 * M_PI;
                a->bode.ph[a->bode_idx] = ph * 180.0 / M_PI;
                if (a->bode.n < a->bode_idx + 1) a->bode.n = a->bode_idx + 1;
            }
        }
        a->bode_idx++;
        if (a->bode_idx >= a->bode_points) {
            a->bode_active = 0;
            app_set_message(a, "扫频完成（%d 点）", a->bode.n);
        } else {
            a->gen.freq = a->bode_f1 *
                          pow(a->bode_f2 / a->bode_f1,
                              (double)a->bode_idx /
                                  (double)(a->bode_points - 1));
            a->eng.gen_cfg = a->gen;
            double settle = 10.0 / AS_MAX(a->gen.freq, 1.0) + 0.25;
            if (settle < 0.3) settle = 0.3;
            a->bode_next = a->now + settle;
        }
    }
}



/* ---- AUTO / reset ----------------------------------------------------- */
static const double TIME_LADDER[15] = {5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4,
                                       1e-3, 2e-3, 5e-3, 1e-2, 2e-2, 5e-2, 1e-1,
                                       2e-1};
static const double VOLT_LADDER[11] = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05,
                                       0.1, 0.2, 0.5, 1.0, 2.0};

static double snap_up(double v, const double *arr, int n) {
    for (int i = 0; i < n; i++)
        if (arr[i] >= v * 0.97) return arr[i];
    return v;
}

void app_reset_view(App *a) {
    a->time_div = 1e-3;
    a->volt_div = 0.2;
    a->v_off = 0.0;
    a->trig_level = 0.0;
    app_set_message(a, "视图已复位：1 ms/格，200 mV/格");
}

void app_zoom_time(App *a, int dir) {
    /* dir > 0 zooms in (smaller time/div) */
    double cur = a->time_div;
    int n = 15;
    if (dir > 0) {
        for (int i = n - 1; i >= 0; i--)
            if (TIME_LADDER[i] < cur * 0.98) {
                a->time_div = TIME_LADDER[i];
                return;
            }
        a->time_div = TIME_LADDER[0];
    } else {
        for (int i = 0; i < n; i++)
            if (TIME_LADDER[i] > cur * 1.02) {
                a->time_div = TIME_LADDER[i];
                return;
            }
        a->time_div = TIME_LADDER[n - 1];
    }
}

void app_zoom_volt(App *a, int dir) {
    /* dir > 0 zooms in (smaller volts/div) */
    double cur = a->volt_div;
    int n = 11;
    if (dir > 0) {
        for (int i = 0; i < n; i++)
            if (VOLT_LADDER[i] < cur * 0.98) {
                a->volt_div = VOLT_LADDER[i];
                return;
            }
        a->volt_div = VOLT_LADDER[0];
    } else {
        for (int i = n - 1; i >= 0; i--)
            if (VOLT_LADDER[i] > cur * 1.02) {
                a->volt_div = VOLT_LADDER[i];
                return;
            }
        a->volt_div = VOLT_LADDER[n - 1];
    }
}

void app_auto_setup(App *a) {
    if (!a->running) {
        app_start(a);
        if (!a->running) return;
    }
    a->frozen = 0;
    double fs = a->fs;
    int need = (int)(fs * 0.5);
    if (need < 8192) need = 8192;
    if (need > a->cap_frames) need = a->cap_frames;
    int64_t got = engine_read(&a->eng, a->cap, need);
    if (got < 2048) {
        app_set_message(a, "AUTO：数据不足，请稍后重试");
        return;
    }
    int total = (int)got;
    double m0 = 0, m1 = 0;
    for (int i = 0; i < total; i++) {
        m0 += a->cap[2 * i];
        m1 += a->cap[2 * i + 1];
    }
    m0 /= total;
    m1 /= total;
    double vp0 = -1e9, vn0 = 1e9, vp1 = -1e9, vn1 = 1e9;
    for (int i = 0; i < total; i++) {
        double v0 = a->cap[2 * i] - m0;
        double v1 = a->cap[2 * i + 1] - m1;
        a->mono1[i] = (float)v0;
        a->mono2[i] = (float)v1;
        if (v0 > vp0) vp0 = v0;
        if (v0 < vn0) vn0 = v0;
        if (v1 > vp1) vp1 = v1;
        if (v1 < vn1) vn1 = v1;
    }
    double pp0 = vp0 - vn0, pp1 = vp1 - vn1;
    int chans[2], nch = 0;
    if (a->show_ch[0]) chans[nch++] = 0;
    if (a->show_ch[1]) chans[nch++] = 1;
    if (nch == 0) {
        chans[0] = 0;
        chans[1] = 1;
        nch = 2;
    }
    int main_ch = chans[0];
    for (int i = 1; i < nch; i++)
        if ((chans[i] == 0 ? pp0 : pp1) > (main_ch == 0 ? pp0 : pp1))
            main_ch = chans[i];
    double main_pp = (main_ch == 0) ? pp0 : pp1;
    if (main_pp < 2e-5) {
        app_set_message(a, "AUTO：没有检测到信号（检查接线、输入增益与输出内容）");
        return;
    }

    TimeStats st;
    time_stats((main_ch == 0) ? a->mono1 : a->mono2, total, fs, 1.0, &st);
    double freq = st.freq;
    double win_s;
    char desc[96];
    if (freq > 5.0 && freq < fs * 0.45) {
        win_s = 3.0 / freq;
        snprintf(desc, sizeof(desc), "%.4g Hz（约 3 个周期）", freq);
    } else {
        win_s = 0.01;
        snprintf(desc, sizeof(desc), "周期未知，使用 10 ms 窗口");
    }
    win_s = AS_CLAMP(win_s, 20.0 / fs, 0.4);
    a->time_div = snap_up(win_s / 10.0, TIME_LADDER, 15);

    double sc0 = a->volts_at_fs * a->ch_gain[0];
    double sc1 = a->volts_at_fs * a->ch_gain[1];
    double target = 0;
    for (int i = 0; i < nch; i++) {
        double pp = (chans[i] == 0 ? pp0 : pp1) * (chans[i] == 0 ? sc0 : sc1);
        if (pp > target) target = pp;
    }
    a->volt_div = snap_up(AS_MAX(target, 1e-9) / 5.0, VOLT_LADDER, 11);
    a->trig_src = main_ch;
    a->trig_edge = 0;
    a->trig_level = 0.5 * (main_ch == 0 ? (vp0 + vn0) : (vp1 + vn1));
    a->trig_hyst = AS_MAX(0.005, 0.05 * main_pp);
    a->v_off = -(main_ch == 0 ? m0 : m1) * (main_ch == 0 ? sc0 : sc1);
    char b1[32], b2[32];
    as_eng(b1, sizeof(b1), a->time_div, "s", 3);
    as_eng(b2, sizeof(b2), a->volt_div, "V", 3);
    app_set_message(a, "AUTO：%s ｜ 时基 %s/格 ｜ 垂直 %s/格 ｜ 触发 CH%d", desc, b1,
                    b2, main_ch + 1);
    a->tab = TAB_SCOPE;
}

/* ---- exports ---------------------------------------------------------- */
static void put_u32le(FILE *f, uint32_t v) {
    fputc((int)(v & 0xff), f);
    fputc((int)((v >> 8) & 0xff), f);
    fputc((int)((v >> 16) & 0xff), f);
    fputc((int)((v >> 24) & 0xff), f);
}
static void put_u16le(FILE *f, uint16_t v) {
    fputc((int)(v & 0xff), f);
    fputc((int)((v >> 8) & 0xff), f);
}

int app_export_wav(App *a, const char *path) {
    if (!a->running) {
        app_set_message(a, "未开始采集，无法保存");
        return -1;
    }
    int want = (int)(a->fs * 1.0);
    if (want > a->cap_frames) want = a->cap_frames;
    int64_t got = engine_read(&a->eng, a->cap, want);
    if (got < 64) {
        app_set_message(a, "没有采集到数据");
        return -1;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        app_set_message(a, "无法写入文件");
        return -1;
    }
    uint32_t frames = (uint32_t)got;
    uint32_t data_bytes = frames * 2 * 3;
    fwrite("RIFF", 1, 4, f);
    put_u32le(f, 36 + data_bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put_u32le(f, 16);
    put_u16le(f, 1);          /* PCM */
    put_u16le(f, 2);          /* channels */
    put_u32le(f, (uint32_t)a->fs);
    put_u32le(f, (uint32_t)a->fs * 6);
    put_u16le(f, 6);
    put_u16le(f, 24);
    fwrite("data", 1, 4, f);
    put_u32le(f, data_bytes);
    for (uint32_t i = 0; i < frames * 2; i++) {
        double v = a->cap[i];
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        int32_t s = (int32_t)(v * 8388607.0);
        fputc((int)(s & 0xff), f);
        fputc((int)((s >> 8) & 0xff), f);
        fputc((int)((s >> 16) & 0xff), f);
    }
    fclose(f);
    app_set_message(a, "已保存 WAV: %.2f s", (double)frames / a->fs);
    return 0;
}

int app_export_csv(App *a, const char *path) {
    if (!a->running) {
        app_set_message(a, "未开始采集，无法保存");
        return -1;
    }
    int want = (int)(a->fs * 0.5);
    if (want > a->cap_frames) want = a->cap_frames;
    int64_t got = engine_read(&a->eng, a->cap, want);
    if (got < 64) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "time_s,ch1,ch2\n");
    for (int64_t i = 0; i < got; i++)
        fprintf(f, "%.9f,%.7f,%.7f\n", (double)i / a->fs, a->cap[2 * i],
                a->cap[2 * i + 1]);
    fclose(f);
    app_set_message(a, "已保存 CSV（%lld 点）", (long long)got);
    return 0;
}

int app_export_bode(App *a, const char *path) {
    if (a->bode.n <= 0) {
        app_set_message(a, "还没有扫频数据");
        return -1;
    }
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "freq_hz,mag_db,phase_deg\n");
    for (int i = 0; i < a->bode.n; i++)
        fprintf(f, "%.6g,%.4f,%.4f\n", a->bode.f[i], a->bode.g[i],
                a->bode.ph[i]);
    fclose(f);
    app_set_message(a, "已保存频响 CSV（%d 点）", a->bode.n);
    return 0;
}

