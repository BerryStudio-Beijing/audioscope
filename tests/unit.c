/* AudioScope 1.1 — 单元测试（波形、分析、校准、报告、虚拟闭环）。
 *
 * 这些测试不打开声卡，全部可重复：
 *  - 频率/幅度误差用 ppm 与 dB 量化；
 *  - 抗混叠用“最强非谐波杂散”量化；
 *  - 分析链用一个已知传递函数的虚拟 DUT 端到端验证。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "analyze.h"
#include "calib.h"
#include "dsp.h"
#include "report.h"
#include "wavegen.h"

static int g_pass, g_fail;
static const char *g_group = "";

static void group(const char *s) {
    g_group = s;
    printf("\n[%s]\n", s);
}

static void check(int cond, const char *name, const char *detail) {
    if (cond) {
        g_pass++;
        printf("  ok   %-42s %s\n", name, detail ? detail : "");
    } else {
        g_fail++;
        printf("  FAIL %-42s %s\n", name, detail ? detail : "");
    }
}

static void check_le(double got, double limit, const char *name,
                     const char *unit) {
    char d[96];
    snprintf(d, sizeof(d), "%.6g %s (limit %.6g)", got, unit, limit);
    check(fabs(got) <= limit, name, d);
}

/* ------------------------------------------------------------------ */
static void test_determinism(void) {
    group("确定性 / determinism");
    double fs = 48000;
    int n = 4096;
    float *a = malloc(n * sizeof(float)), *b = malloc(n * sizeof(float));
    int all_same = 1;
    for (int w = 0; w < WG_COUNT; w++) {
        WgConfig c;
        wg_default(&c, fs);
        c.wave = w;
        c.f1 = c.f2 = 997.0;
        c.amp = 0.3;
        c.seed = 12345;
        WgState s1, s2;
        wg_init(&s1, &c);
        wg_init(&s2, &c);
        wg_render(&s1, a, n);
        wg_render(&s2, b, n);
        if (memcmp(a, b, (size_t)n * sizeof(float)) != 0) all_same = 0;
    }
    check(all_same, "同参数两次渲染逐位相同", "9 种波形");

    /* 换种子必须不同（噪声） */
    WgConfig c1, c2;
    wg_default(&c1, fs);
    c1.wave = WG_WHITE;
    c2 = c1;
    c2.seed = 999;
    WgState s1, s2;
    wg_init(&s1, &c1);
    wg_init(&s2, &c2);
    wg_render(&s1, a, n);
    wg_render(&s2, b, n);
    check(memcmp(a, b, (size_t)n * sizeof(float)) != 0, "不同种子产生不同噪声",
          "xorshift64*");
    free(a);
    free(b);
}

static void test_accuracy(void) {
    group("波形精度 / accuracy");
    double fs = 48000;
    double f_list[6] = {20, 100, 440, 1000, 5000, 15000};
    double worst_ppm = 0, worst_db = 0;
    for (int i = 0; i < 6; i++) {
        WgConfig c;
        wg_default(&c, fs);
        c.wave = WG_SINE;
        c.f1 = c.f2 = f_list[i];
        c.amp = as_lin(-6.0);
        WgCheck k;
        if (wg_check(&c, 65536, &k) != 0) {
            check(0, "wg_check", "alloc");
            return;
        }
        if (fabs(k.freq_err_ppm) > fabs(worst_ppm)) worst_ppm = k.freq_err_ppm;
        if (fabs(k.amp_err_db) > fabs(worst_db)) worst_db = k.amp_err_db;
        char name[64];
        snprintf(name, sizeof(name), "正弦 %.0f Hz 频率误差", f_list[i]);
        check_le(k.freq_err_ppm, 50.0, name, "ppm");
    }
    char d[96];
    snprintf(d, sizeof(d), "最差 %.1f ppm / %.4f dB", worst_ppm, worst_db);
    check(fabs(worst_db) <= 0.01, "正弦幅度误差 ≤ 0.01 dB", d);

    /* 三角波：闭式，幅度应精确 */
    WgConfig t;
    wg_default(&t, fs);
    t.wave = WG_TRIANGLE;
    t.f1 = t.f2 = 1000;
    t.amp = 0.5;
    WgCheck k;
    wg_check(&t, 32768, &k);
    snprintf(d, sizeof(d), "%.4f dB", k.amp_err_db);
    check(fabs(k.amp_err_db) <= 0.02, "三角波幅度误差 ≤ 0.02 dB", d);

    /* 抗混叠：方波/锯齿的非谐波杂散应显著低于基波 */
    WgConfig sq;
    wg_default(&sq, fs);
    sq.wave = WG_SQUARE;
    sq.f1 = sq.f2 = 1000;
    sq.amp = 0.5;
    wg_check(&sq, 32768, &k);
    snprintf(d, sizeof(d), "spur %.1f dBc, THD %.1f%%", k.spur_dbc,
             k.thd_percent);
    check(k.spur_dbc < -18.0, "方波 PolyBLEP 抑制混叠 (< -18 dBc)", d);

    WgConfig tr;
    wg_default(&tr, fs);
    tr.wave = WG_TRIANGLE;
    tr.f1 = tr.f2 = 1000;
    tr.amp = 0.5;
    wg_check(&tr, 32768, &k);
    snprintf(d, sizeof(d), "spur %.1f dBc", k.spur_dbc);
    check(k.spur_dbc < -35.0, "三角波谐波按 1/n² 衰减 (< -35 dBc)", d);

    WgConfig sn;
    wg_default(&sn, fs);
    sn.wave = WG_SINE;
    sn.f1 = sn.f2 = 1000;
    sn.amp = 0.5;
    wg_check(&sn, 32768, &k);
    snprintf(d, sizeof(d), "spur %.1f dBc", k.spur_dbc);
    check(k.spur_dbc < -90.0, "正弦纯净度 (< -90 dBc)", d);
}

/* ------------------------------------------------------------------ */
static void test_analysis(void) {
    group("分析 / analysis");
    double fs = 48000;
    int n = 32768;

    /* 1. 单频幅度/相位 */
    float *x = malloc((size_t)n * sizeof(float));
    double A = 0.25, phi = 37.0;
    for (int i = 0; i < n; i++)
        x[i] = (float)(A * sin(2.0 * AS_PI * 997.0 * i / fs + phi * AS_PI / 180.0));
    double ph = 0;
    double amp = an_tone(x, n, fs, 997.0, &ph);
    char d[128];
    snprintf(d, sizeof(d), "amp %.5f (期望 %.5f), phase %.2f° (期望 %.2f°)", amp,
             A, ph, phi);
    check(fabs(amp - A) / A < 0.005 && fabs(ph - phi) < 1.0, "单频幅度/相位", d);

    /* 2. 互相关延迟：延迟 123.4 个样本 */
    int nr = n + 4096;
    float *rx = calloc((size_t)nr, sizeof(float));
    double delay = 123.4;
    int di = (int)delay;
    double fr = delay - di;
    for (int i = 0; i < n - 1; i++) {
        double v = 0.4 * sin(2.0 * AS_PI * 1500.0 * i / fs);
        rx[i + di] += (float)(v * (1.0 - fr));
        rx[i + di + 1] += (float)(v * fr);
    }
    double peak = 0;
    double got = an_xcorr_delay(x, 0, rx, nr, &peak);   /* ntx=0 -> 0 */
    (void)got;
    float *ref = malloc((size_t)n * sizeof(float));
    for (int i = 0; i < n; i++)
        ref[i] = (float)(0.4 * sin(2.0 * AS_PI * 1500.0 * i / fs));
    got = an_xcorr_delay(ref, 8192, rx, nr, &peak);
    snprintf(d, sizeof(d), "%.2f 样本 (期望 %.2f, 相关峰 %.3f)", got, delay, peak);
    check(fabs(got - delay) < 0.6 && peak > 0.9, "互相关延迟对齐", d);

    /* 3. 同步平均降噪：16 段折叠应让噪声底下降约 12 dB */
    int period = 480;
    int total = period * 16;
    float *seg = malloc((size_t)total * sizeof(float));
    float *avg = malloc((size_t)period * sizeof(float));
    unsigned seed = 7;
    for (int i = 0; i < total; i++) {
        seed = seed * 1664525u + 1013904223u;
        seg[i] = (float)(((double)(seed >> 8) / 16777216.0 - 0.5) * 0.02);
    }
    an_sync_average(seg, total, period, avg, period);
    double nb = an_noise_floor_dbfs(seg, total);
    double na = an_noise_floor_dbfs(avg, period);
    double drop = nb - na;
    snprintf(d, sizeof(d), "%.1f dB (理论 12.0 dB)", drop);
    check(drop > 8.0 && drop < 16.0, "同步平均降噪", d);

    /* 3b. 折叠不能改变周期性信号本身 */
    for (int i = 0; i < total; i++)
        seg[i] += (float)(0.5 * sin(2.0 * AS_PI * 100.0 * i / fs));
    an_sync_average(seg, total, period, avg, period);
    float mn = avg[0], mx = avg[0];
    for (int i = 1; i < period; i++) {
        if (avg[i] < mn) mn = avg[i];
        if (avg[i] > mx) mx = avg[i];
    }
    snprintf(d, sizeof(d), "峰峰值 %.3f (期望 1.000)", mx - mn);
    check(fabs((mx - mn) - 1.0) < 0.01, "同步平均保留周期信号", d);

    /* 4. 上升沿：一阶 RC（fc = 2 kHz）的离散 10-90% 时间 */
    int ns = 2048;
    double fc = 2000.0, tau = 1.0 / (2.0 * AS_PI * fc);
    double k = (1.0 / fs) / tau;
    if (k > 0.6) k = 0.6;
    double y = 0;
    for (int i = 0; i < ns; i++) {
        double target = (i > 200) ? 1.0 : 0.0;
        y += (target - y) * k;
        seg[i] = (float)y;
    }
    double rise = an_rise_time_us(seg, ns, fs);
    double expect = fabs((log(0.9) - log(0.1)) / log(1.0 - k)) / fs * 1e6;
    snprintf(d, sizeof(d), "%.1f µs (离散理论 %.1f)", rise, expect);
    check(fabs(rise - expect) / expect < 0.02, "上升沿测量", d);

    free(x);
    free(rx);
    free(ref);
    free(seg);
    free(avg);
}

static void test_calib_and_report(void) {
    group("校准与报告 / calib + report");
    CalibData c;
    calib_clear(&c);
    c.valid = 1;
    c.fs = 48000;
    c.n = 4;
    double f[4] = {20, 200, 2000, 20000};
    double g[4] = {-0.4, 0.1, 0.0, -1.2};
    for (int i = 0; i < 4; i++) {
        c.f[i] = f[i];
        c.gain_db[i] = g[i];
        c.phase_deg[i] = -i * 5.0;
    }
    c.delay_ms = 8.4;
    mkdir("build/sim", 0755);
    check(calib_save(&c, "build/sim/ut_cal.json") == 0, "校准写入", "build/sim");
    CalibData r;
    int ok = calib_load(&r, "build/sim/ut_cal.json") == 0;
    char d[128];
    snprintf(d, sizeof(d), "n=%d delay=%.2f g(200)=%.3f", r.n, r.delay_ms,
             calib_gain_at(&r, 200));
    check(ok && r.n == 4 && fabs(r.delay_ms - 8.4) < 1e-6 &&
              fabs(calib_gain_at(&r, 200) - 0.1) < 1e-3,
          "校准读写与插值", d);

    Report rep;
    memset(&rep, 0, sizeof(rep));
    rep.fs = 48000;
    rep.block = 512;
    rep.level_dbfs = -12;
    rep.points = 4;
    rep.f1 = 20;
    rep.f2 = 20000;
    rep.tol_db = 0.5;
    rep.nfr = 4;
    for (int i = 0; i < 4; i++) {
        rep.fr[i].f = f[i];
        rep.fr[i].gain_db = g[i];
        rep.fr[i].phase_deg = c.phase_deg[i];
    }
    rep.res.have_thd = 1;
    rep.res.thdn_percent = 0.05;
    rep.res.have_noise = 1;
    rep.res.noise_dbfs = -92.0;
    rep.res.have_delay = 1;
    rep.res.delay_ms = 8.4;
    rep.res.max_dev_db = -1.2;
    rep.res.max_dev_f = 20000;
    rep.res.flatness_db = 1.3;
    rep.settings_hash = 0x12345678;
    report_build(&rep, &c);
    check(report_json(&rep, "build/sim/ut_a.json") == 0 &&
              report_json(&rep, "build/sim/ut_b.json") == 0,
          "报告写入", "JSON ×2");
    FILE *fa = fopen("build/sim/ut_a.json", "rb");
    FILE *fb = fopen("build/sim/ut_b.json", "rb");
    int same = fa && fb;
    if (same) {
        int ca, cb;
        do {
            ca = fgetc(fa);
            cb = fgetc(fb);
            if (ca != cb) {
                same = 0;
                break;
            }
        } while (ca != EOF);
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    check(same, "同参数报告逐字节一致", "无时间戳，仅设置哈希");
    check(rep.nadvice > 0, "报告给出调整建议", rep.verdict_text);
}

/* ------------------------------------------------------------------ */
/* 虚拟 DUT：二阶低通 3 kHz + 6.2 ms 延迟 + -96 dBFS 噪声              */
/* ------------------------------------------------------------------ */
typedef struct {
    double fc, delay_ms, noise_dbfs, thd; /* thd = 二次谐波相对幅度 */
} VirtualDut;

static double dut_gain_db(const VirtualDut *d, double f) {
    double x = f / d->fc;
    return -10.0 * log10(1.0 + x * x * x * x);
}
static double dut_phase_deg(const VirtualDut *d, double f) {
    double x = f / d->fc;
    return -atan2(2.0 * x, 1.0 - x * x) * 180.0 / AS_PI;
}


static void mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

int run_simulate(const char *dir) {
    mkdir_p(dir);
    double fs = 48000;
    VirtualDut dut = {3000.0, 6.2, -96.0, 0.0008};
    int n = 32768;
    float *cap = malloc((size_t)n * sizeof(float));
    double level_db = -12.0;
    double amp = as_lin(level_db);
    int points = 24;
    FrPoint fr[AN_MAX_FR];
    int nfr = 0;
    unsigned seed = 20260101u;

    printf("virtual DUT: 2nd-order LPF %.0f Hz, delay %.1f ms, noise %.0f dBFS\n",
           dut.fc, dut.delay_ms, dut.noise_dbfs);
    printf("%10s %10s %10s %10s %10s\n", "f(Hz)", "gain(dB)", "model(dB)",
           "err(dB)", "phase(°)");
    double worst = 0;
    double lr = log(20000.0 / 20.0);
    for (int i = 0; i < points; i++) {
        double f = 20.0 * exp(lr * i / (points - 1));
        double g = dut_gain_db(&dut, f);
        double ph = dut_phase_deg(&dut, f) - 360.0 * dut.delay_ms * 1e-3 * f;
        while (ph < -180.0) ph += 360.0;
        double a = amp * as_lin(g);
        for (int k = 0; k < n; k++) {
            seed = seed * 1664525u + 1013904223u;
            double noise = ((double)(seed >> 8) / 16777216.0 - 0.5) *
                           as_lin(dut.noise_dbfs) * 3.4641016;
            double v = a * sin(2.0 * AS_PI * f * k / fs + ph * AS_PI / 180.0);
            v += dut.thd * a * sin(2.0 * AS_PI * 2.0 * f * k / fs + 30.0);
            cap[k] = (float)(v + noise);
        }
        double mph = 0;
        double mamp = an_tone(cap, n, fs, f, &mph);
        double mg = as_db(mamp / amp);
        fr[nfr].f = f;
        fr[nfr].gain_db = mg;
        fr[nfr].gain_raw_db = mg;
        fr[nfr].phase_deg = mph;
        fr[nfr].amp_err_db = as_db(mamp) - level_db;
        worst = AS_MAX(worst, fabs(mg - g));
        printf("%10.1f %10.3f %10.3f %10.3f %10.1f\n", f, mg, g, mg - g, mph);
        nfr++;
    }

    Report rep;
    memset(&rep, 0, sizeof(rep));
    snprintf(rep.in_dev, sizeof(rep.in_dev), "virtual-input");
    snprintf(rep.out_dev, sizeof(rep.out_dev), "virtual-output");
    rep.fs = fs;
    rep.block = 512;
    rep.channels = 1;
    rep.level_dbfs = level_db;
    rep.points = points;
    rep.f1 = 20;
    rep.f2 = 20000;
    rep.tol_db = 0.5;
    rep.settings_hash = 0x5EED1234u;
    rep.nfr = nfr;
    for (int i = 0; i < nfr; i++) rep.fr[i] = fr[i];
    an_evaluate(fr, nfr, NULL, &rep.res, rep.tol_db);

    /* 失真与噪声用 1 kHz 单独测一次 */
    double a1 = amp * as_lin(dut_gain_db(&dut, 1000.0));
    for (int k = 0; k < n; k++) {
        seed = seed * 1664525u + 1013904223u;
        double noise = ((double)(seed >> 8) / 16777216.0 - 0.5) *
                       as_lin(dut.noise_dbfs) * 3.4641016;
        cap[k] = (float)(a1 * sin(2.0 * AS_PI * 1000.0 * k / fs) +
                         dut.thd * a1 * sin(2.0 * AS_PI * 2000.0 * k / fs) + noise);
    }
    FFT fft;
    if (fft_init(&fft, 8192) == 0) {
        double *x = malloc(8192 * sizeof(double));
        for (int i = 0; i < 8192; i++) x[i] = cap[i];
        ToneMetrics tm;
        if (analyze_tone(&fft, x, 8192, fs, 1000.0, WIN_BH, &tm)) {
            rep.res.thd_percent = tm.thd * 100.0;
            rep.res.thdn_percent = tm.thdn * 100.0;
            rep.res.snr_db = tm.snr;
            rep.res.sinad_db = tm.sinad;
            rep.res.have_thd = 1;
        }
        free(x);
        fft_free(&fft);
    }
    for (int i = 0; i < n; i++) cap[i] = 0;
    for (int k = 0; k < n; k++) {
        seed = seed * 1664525u + 1013904223u;
        cap[k] = (float)(((double)(seed >> 8) / 16777216.0 - 0.5) *
                         as_lin(dut.noise_dbfs) * 3.4641016);
    }
    rep.res.noise_dbfs = an_noise_floor_dbfs(cap, n);
    rep.res.have_noise = 1;
    rep.res.delay_ms = dut.delay_ms;
    rep.res.have_delay = 1;
    rep.verdict = 0;
    report_build(&rep, NULL);

    char p[1024];
    snprintf(p, sizeof(p), "%s/report.json", dir);
    if (report_json(&rep, p) != 0) {
        printf("cannot write %s\n", p);
        free(cap);
        return 1;
    }
    snprintf(p, sizeof(p), "%s/fr.csv", dir);
    report_csv(&rep, p);
    snprintf(p, sizeof(p), "%s/capture.wav", dir);
    report_wav24(cap, n, fs, p);

    printf("\nTHD %.4f%%  THD+N %.4f%%  SNR %.1f dB  noise %.1f dBFS\n",
           rep.res.thd_percent, rep.res.thdn_percent, rep.res.snr_db,
           rep.res.noise_dbfs);
    printf("%s\n", rep.verdict_text);
    printf("worst |gain - model| = %.3f dB (tolerance 0.20 dB)\n", worst);
    printf("artifacts: %s\n", dir);
    free(cap);
    return worst <= 0.20 ? 0 : 1;
}

int run_unittest(void) {
    g_pass = g_fail = 0;
    printf("AudioScope 1.1 unit tests\n");
    test_determinism();
    test_accuracy();
    test_analysis();
    test_calib_and_report();
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

