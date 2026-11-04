#include "report.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dsp.h"

static void json_str(FILE *f, const char *s) {
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') fprintf(f, "\\%c", *p);
        else if (*p == '\n') fputs("\\n", f);
        else if (*p < 0x20) fprintf(f, "\\u%04x", *p);
        else fputc(*p, f);
    }
    fputc('"', f);
}

static double snap(double v, int decimals) {
    if (!isfinite(v)) return 0.0;
    double m = pow(10.0, decimals);
    return round(v * m) / m;
}

int report_json(const Report *r, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\n  \"schema\": \"audioscope-report/1.1\",\n");
    fprintf(f, "  \"settings\": {\n");
    fprintf(f, "    \"input\": ");
    json_str(f, r->in_dev);
    fprintf(f, ",\n    \"output\": ");
    json_str(f, r->out_dev);
    fprintf(f, ",\n    \"sample_rate\": %.0f,\n    \"block\": %d,\n", r->fs,
            r->block);
    fprintf(f, "    \"channels\": %d,\n    \"level_dbfs\": %.3f,\n",
            r->channels, snap(r->level_dbfs, 3));
    fprintf(f, "    \"points\": %d,\n    \"freq_start_hz\": %.6g,\n"
               "    \"freq_stop_hz\": %.6g,\n",
            r->points, snap(r->f1, 6), snap(r->f2, 6));
    fprintf(f, "    \"use_calibration\": %s,\n    \"tolerance_db\": %.3f,\n",
            r->use_calib ? "true" : "false", snap(r->tol_db, 3));
    fprintf(f, "    \"settings_hash\": \"%08x\"\n  },\n", r->settings_hash);

    fprintf(f, "  \"results\": {\n");
    fprintf(f, "    \"fr_flatness_db\": %.3f,\n", snap(r->res.flatness_db, 3));
    fprintf(f, "    \"max_deviation_db\": %.3f,\n",
            snap(r->res.max_dev_db, 3));
    fprintf(f, "    \"max_deviation_hz\": %.6g,\n",
            snap(r->res.max_dev_f, 6));
    fprintf(f, "    \"phase_deviation_deg\": %.2f,\n",
            snap(r->res.phase_dev_deg, 2));
    fprintf(f, "    \"thd_percent\": %.4f,\n", snap(r->res.thd_percent, 4));
    fprintf(f, "    \"thdn_percent\": %.4f,\n", snap(r->res.thdn_percent, 4));
    fprintf(f, "    \"snr_db\": %.2f,\n", snap(r->res.snr_db, 2));
    fprintf(f, "    \"sinad_db\": %.2f,\n", snap(r->res.sinad_db, 2));
    fprintf(f, "    \"noise_floor_dbfs\": %.2f,\n",
            snap(r->res.noise_dbfs, 2));
    fprintf(f, "    \"latency_ms\": %.3f,\n", snap(r->res.delay_ms, 3));
    fprintf(f, "    \"rise_time_us\": %.2f,\n", snap(r->res.rise_us, 2));
    fprintf(f, "    \"clipping_samples\": %d,\n", r->res.clip_count);
    fprintf(f, "    \"verdict\": \"%s\"\n  },\n", r->verdict ? "pass" : "fail");

    fprintf(f, "  \"frequency_response\": [\n");
    for (int i = 0; i < r->nfr; i++) {
        fprintf(f, "    {\"f\": %.6g, \"gain_db\": %.3f, \"phase_deg\": %.2f}%s\n",
                snap(r->fr[i].f, 6), snap(r->fr[i].gain_db, 3),
                snap(r->fr[i].phase_deg, 2), i + 1 < r->nfr ? "," : "");
    }
    fprintf(f, "  ],\n  \"advice\": [\n");
    for (int i = 0; i < r->nadvice; i++) {
        fprintf(f, "    ");
        json_str(f, r->advice[i]);
        fprintf(f, "%s\n", i + 1 < r->nadvice ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    return 0;
}

int report_csv(const Report *r, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "freq_hz,gain_db,phase_deg,amp_err_db\n");
    for (int i = 0; i < r->nfr; i++)
        fprintf(f, "%.6g,%.3f,%.2f,%.3f\n", r->fr[i].f, r->fr[i].gain_db,
                r->fr[i].phase_deg, r->fr[i].amp_err_db);
    fclose(f);
    return 0;
}

int report_wav24(const float *x, int n, double fs, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t data_bytes = (uint32_t)n * 3;
    fwrite("RIFF", 1, 4, f);
    uint32_t v = 36 + data_bytes;
    for (int i = 0; i < 4; i++) fputc((int)((v >> (8 * i)) & 0xff), f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    for (int i = 0; i < 4; i++) fputc((int)((v >> (8 * i)) & 0xff), f);
    fputc(1, f);
    fputc(0, f);
    fputc(1, f);
    fputc(0, f);
    v = (uint32_t)fs;
    for (int i = 0; i < 4; i++) fputc((int)((v >> (8 * i)) & 0xff), f);
    v = (uint32_t)fs * 3;
    for (int i = 0; i < 4; i++) fputc((int)((v >> (8 * i)) & 0xff), f);
    fputc(3, f);
    fputc(0, f);
    fputc(24, f);
    fputc(0, f);
    fwrite("data", 1, 4, f);
    for (int i = 0; i < 4; i++) fputc((int)((data_bytes >> (8 * i)) & 0xff), f);
    for (int i = 0; i < n; i++) {
        double s = x[i];
        if (s > 1) s = 1;
        if (s < -1) s = -1;
        int32_t q = (int32_t)(s * 8388607.0);
        fputc((int)(q & 0xff), f);
        fputc((int)((q >> 8) & 0xff), f);
        fputc((int)((q >> 16) & 0xff), f);
    }
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* 规则式建议：只根据测量值推理，不联网、不调用任何外部服务           */
/* ------------------------------------------------------------------ */
static void add_advice(Report *r, const char *fmt, ...) {
    if (r->nadvice >= 12) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->advice[r->nadvice], sizeof(r->advice[0]), fmt, ap);
    va_end(ap);
    r->nadvice++;
}

void report_build(Report *r, const CalibData *cal) {
    r->nadvice = 0;
    AnResult *a = &r->res;
    double tol = r->tol_db > 0 ? r->tol_db : 0.5;

    /* 频响最差点 */
    double worstdb = 0, worstf = 0;
    for (int i = 0; i < r->nfr; i++)
        if (fabs(r->fr[i].gain_db) > fabs(worstdb)) {
            worstdb = r->fr[i].gain_db;
            worstf = r->fr[i].f;
        }

    int pass = 1;
    if (r->nfr > 0 && (fabs(worstdb) > tol || a->flatness_db > tol * 2.0))
        pass = 0;
    if (a->have_thd && a->thdn_percent > 1.0) pass = 0;
    if (a->clip_count > 0) pass = 0;
    if (a->have_noise && a->noise_dbfs > -60.0) pass = 0;

    if (a->clip_count > 0) {
        add_advice(r, "检测到 %d 个采样削波：先把激励电平降到 %.0f dBFS（当前 %.1f dBFS），"
                      "并确认输入增益没有顶到。",
                   a->clip_count, AS_MAX(-60.0, r->level_dbfs - 6.0),
                   r->level_dbfs);
    }
    if (r->nfr > 0 && fabs(worstdb) > tol) {
        if (worstf > 8000.0 && worstdb < 0)
            add_advice(r, "高频滚降 %.2f dB @ %.0f Hz：查输出耦合电容/低通 RC（截止点约 %.0f Hz），"
                          "把 C 或 R 按 1/(2πRC) 反推后减小 20%% 再测。",
                       worstdb, worstf, worstf);
        else if (worstf < 200.0 && worstdb < 0)
            add_advice(r, "低频衰减 %.2f dB @ %.0f Hz：查输入/输出耦合电容与变压器，"
                          "按 f = 1/(2πRC) 把耦合电容加大约 %.0f%%。",
                       worstdb, worstf, 100.0 * (pow(10.0, -worstdb / 20.0) - 1.0));
        else if (worstdb > 0)
            add_advice(r, "在 %.0f Hz 有 %.2f dB 抬升：检查该频段的反馈网络/谐振，"
                          "或确认测试电平没有进入非线性区。",
                       worstf, worstdb);
        else
            add_advice(r, "频响在 %.0f Hz 偏差 %.2f dB：先复测确认，再检查该频段对应的"
                          "滤波/耦合网络。",
                       worstf, worstdb);
    }
    if (a->have_thd && a->thdn_percent > 0.5)
        add_advice(r, "THD+N %.3f%% 偏高：检查偏置点、电源退耦和增益级；"
                      "先用 -20 dBFS 复测区分失真与噪声。",
                   a->thdn_percent);
    if (a->have_noise && a->noise_dbfs > -70.0)
        add_advice(r, "噪声底 %.1f dBFS 偏高：检查增益结构、接地与屏蔽；"
                      "必要时在输入端加 20 Hz–20 kHz 带限再测。",
                   a->noise_dbfs);
    if (a->phase_dev_deg > 60.0)
        add_advice(r, "相位在频带内变化 %.0f°：确认低切/耦合网络与极性；"
                      "相位属于相对值，超过 ±180° 会折叠。",
                   a->phase_dev_deg);
    if (a->have_delay && a->delay_ms > 40.0)
        add_advice(r, "往返延迟 %.1f ms 偏大：检查缓冲区设置与是否有额外处理环节。",
                   a->delay_ms);

    /* 下一轮参数 */
    double next_level = r->level_dbfs;
    if (a->clip_count > 0) next_level = AS_MAX(-60.0, r->level_dbfs - 6.0);
    else if (a->have_thd && a->thdn_percent > 0.5)
        next_level = AS_MAX(-60.0, r->level_dbfs - 6.0);
    add_advice(r, "下一轮：电平 %.0f dBFS，%d 点，%.0f–%.0f Hz%s", next_level,
               r->points, r->f1, r->f2,
               (r->nfr > 0 && fabs(worstdb) > tol) ? "，并在最差频段附近加密到 1/6 倍频程" : "");

    if (pass)
        snprintf(r->verdict_text, sizeof(r->verdict_text),
                 "通过：频响 ±%.2f dB 内，THD+N %.3f%%，噪声底 %.1f dBFS",
                 AS_MAX(fabs(worstdb), a->flatness_db * 0.5), a->thdn_percent,
                 a->noise_dbfs);
    else
        snprintf(r->verdict_text, sizeof(r->verdict_text),
                 "未通过：最差 %.2f dB @ %.0f Hz，THD+N %.3f%%，削波 %d 点",
                 worstdb, worstf, a->thdn_percent, a->clip_count);
    r->verdict = pass;
    (void)cal;
}

