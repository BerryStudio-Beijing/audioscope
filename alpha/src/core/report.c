/* AudioScope by BerryStudio 0.7-alpha — measurement report writer. */
#include "core/report.h"
#include "core/pdf.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void iso_now(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

static void json_escape(const char *src, char *dst, size_t n) {
    if (!dst || n == 0) return;
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 2 < n; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c == '\n') {
            dst[j++] = '\\'; dst[j++] = 'n';
        } else if (c == '\r') {
            dst[j++] = '\\'; dst[j++] = 'r';
        } else if (c == '\t') {
            dst[j++] = '\\'; dst[j++] = 't';
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static void html_escape(const char *src, char *dst, size_t n) {
    if (!dst || n == 0) return;
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 6 < n; ++i) {
        switch (src[i]) {
        case '&': memcpy(dst + j, "&amp;", 5); j += 5; break;
        case '<': memcpy(dst + j, "&lt;", 4); j += 4; break;
        case '>': memcpy(dst + j, "&gt;", 4); j += 4; break;
        case '"': memcpy(dst + j, "&quot;", 6); j += 6; break;
        default: dst[j++] = src[i]; break;
        }
    }
    dst[j] = '\0';
}

const char *as_verdict_name(int verdict) {
    switch (verdict) {
    case 0: return "PASS";
    case 1: return "WARN";
    case 2: return "FAIL";
    default: return "?";
    }
}

void as_limits_default(AsLimits *limits) {
    if (!limits) return;
    limits->min_freq_hz = 20.0;
    limits->max_freq_hz = 20000.0;
    limits->max_vpp = 0.0;      /* 0 = no limit */
    limits->min_rms = 0.0;      /* 0 = no limit */
    limits->max_clip_samples = 0;
}

void as_report_init(AsReport *report, const char *device, const char *input_mode,
                    double sample_rate, int frames, double seconds) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->schema_version = 2;
    iso_now(report->timestamp, sizeof(report->timestamp));
    char compact[32];
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(compact, sizeof(compact), "%Y%m%d-%H%M%S", &tmv);
    snprintf(report->run_id, sizeof(report->run_id), "run-%s", compact);
    snprintf(report->platform, sizeof(report->platform), "%s", as_platform_name());
    snprintf(report->device, sizeof(report->device), "%s", device ? device : "default");
    snprintf(report->input_mode, sizeof(report->input_mode), "%s", input_mode ? input_mode : "line-pro");
    report->sample_rate = sample_rate;
    report->frames = frames;
    report->seconds = seconds;
    report->confidence = AS_CONF_ASSUMED;
    report->fs_volts_rms = 0.0;
    as_limits_default(&report->limits);
    report->repeat_count = 1;
    report->verdict = 0;
    snprintf(report->verdict_text, sizeof(report->verdict_text), "%s", "PASS");
}

int as_report_evaluate(AsReport *report) {
    if (!report) return AS_ERR_INVALID;
    int fail = 0, warn = 0;
    char reason[192];
    reason[0] = '\0';

    if (report->confidence == AS_CONF_ASSUMED || report->fs_volts_rms <= 0.0) {
        warn = 1;
        snprintf(reason, sizeof(reason), "uncalibrated voltage (assumed FS)");
    }
    if (report->channels <= 0) {
        report->verdict = 2;
        snprintf(report->verdict_text, sizeof(report->verdict_text), "FAIL: no channels measured");
        return AS_OK;
    }

    for (int i = 0; i < report->channels; ++i) {
        const AsChannelMeasurement *m = &report->ch[i];
        if (m->stats.rms < 1e-9) {
            fail = 1;
            snprintf(reason, sizeof(reason), "%s: no signal", m->label);
        }
        if (report->limits.max_clip_samples >= 0 &&
            m->clip_count > report->limits.max_clip_samples) {
            fail = 1;
            snprintf(reason, sizeof(reason), "%s: clip samples %d", m->label, m->clip_count);
        }
        if (report->limits.max_vpp > 0.0 && m->vpp > report->limits.max_vpp) {
            fail = 1;
            snprintf(reason, sizeof(reason), "%s: Vpp %.3f over limit %.3f",
                     m->label, m->vpp, report->limits.max_vpp);
        }
        if (report->limits.min_rms > 0.0 && m->stats.rms < report->limits.min_rms) {
            fail = 1;
            snprintf(reason, sizeof(reason), "%s: RMS %.3f below limit %.3f",
                     m->label, m->stats.rms, report->limits.min_rms);
        }
        if (m->rms_dbfs < -50.0 && m->clip_count == 0) {
            warn = 1;
            if (reason[0] == '\0')
                snprintf(reason, sizeof(reason), "%s: low signal %.1f dBFS",
                         m->label, m->rms_dbfs);
        }
        if (m->stats.freq > 0.0 &&
            (m->stats.freq < report->limits.min_freq_hz ||
             m->stats.freq > report->limits.max_freq_hz)) {
            warn = 1;
            if (reason[0] == '\0')
                snprintf(reason, sizeof(reason), "%s: frequency %.1f Hz outside limits",
                         m->label, m->stats.freq);
        }
    }

    if (fail) {
        report->verdict = 2;
        snprintf(report->verdict_text, sizeof(report->verdict_text), "FAIL: %s",
                 reason[0] ? reason : "limit exceeded");
    } else if (warn) {
        report->verdict = 1;
        snprintf(report->verdict_text, sizeof(report->verdict_text), "WARN: %s",
                 reason[0] ? reason : "review measurement");
    } else {
        report->verdict = 0;
        snprintf(report->verdict_text, sizeof(report->verdict_text), "%s", "PASS");
    }
    return AS_OK;
}

static int atomic_open(const char *path, char *tmp, size_t tmp_len, FILE **out) {
    if (!path || !path[0] || !out) return AS_ERR_INVALID;
    snprintf(tmp, tmp_len, "%s.tmp", path);
    *out = fopen(tmp, "wb");
    return *out ? AS_OK : AS_ERR_IO;
}

static int atomic_commit(FILE *f, const char *path, const char *tmp) {
    if (!f) return AS_ERR_IO;
    int ok = fclose(f) == 0;
    if (!ok) {
        remove(tmp);
        return AS_ERR_IO;
    }
    remove(path);
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return AS_ERR_IO;
    }
    return AS_OK;
}

int as_report_write_json(const AsReport *r, const char *path) {
    if (!r) return AS_ERR_INVALID;
    char tmp[1024];
    FILE *f = NULL;
    int rc = atomic_open(path, tmp, sizeof(tmp), &f);
    if (rc != AS_OK) return rc;

    char device[192], mode[64], calid[192], verdict[256], notes[384];
    json_escape(r->device, device, sizeof(device));
    json_escape(r->input_mode, mode, sizeof(mode));
    json_escape(r->calibration_id, calid, sizeof(calid));
    json_escape(r->verdict_text, verdict, sizeof(verdict));
    json_escape(r->notes, notes, sizeof(notes));

    int ok = fprintf(f,
        "{\n"
        "  \"schema_version\": %d,\n"
        "  \"app\": {\"name\": \"%s\", \"version\": \"%s\"},\n"
        "  \"run_id\": \"%s\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"platform\": \"%s\",\n"
        "  \"device\": \"%s\",\n"
        "  \"input_mode\": \"%s\",\n"
        "  \"sample_rate\": %.0f,\n"
        "  \"frames\": %d,\n"
        "  \"seconds\": %.3f,\n"
        "  \"calibration\": {\"id\": \"%s\", \"confidence\": \"%s\", \"fs_volts_rms\": %.9g},\n"
        "  \"limits\": {\"min_freq_hz\": %.3f, \"max_freq_hz\": %.3f, \"max_vpp\": %.9g, \"min_rms\": %.9g, \"max_clip_samples\": %d},\n"
        "  \"repeat\": {\"count\": %d, \"vpp_std\": %.9g, \"freq_std\": %.9g},\n"
        "  \"channels\": [",
        r->schema_version, as_brand_string(), as_version_string(),
        r->run_id, r->timestamp, r->platform, device, mode,
        r->sample_rate, r->frames, r->seconds,
        calid, as_confidence_name(r->confidence), r->fs_volts_rms,
        r->limits.min_freq_hz, r->limits.max_freq_hz, r->limits.max_vpp,
        r->limits.min_rms, r->limits.max_clip_samples,
        r->repeat_count, r->vpp_std, r->freq_std) > 0;

    for (int i = 0; ok && i < r->channels; ++i) {
        const AsChannelMeasurement *m = &r->ch[i];
        ok = fprintf(f,
            "%s\n    {\"label\": \"%s\", \"channel\": %d, "
            "\"peak_dbfs\": %.6f, \"rms_dbfs\": %.6f, "
            "\"vpp\": %.9g, \"vrms\": %.9g, \"freq_hz\": %.6f, "
            "\"peak\": %.9g, \"rms\": %.9g, \"clip_count\": %d}",
            i ? "," : "", m->label, m->channel,
            m->peak_dbfs, m->rms_dbfs, m->vpp, m->vrms, m->stats.freq,
            m->stats.peak, m->stats.rms, m->clip_count) > 0;
    }
    ok = ok && fprintf(f,
        "\n  ],\n"
        "  \"verdict\": {\"code\": %d, \"text\": \"%s\"},\n"
        "  \"notes\": \"%s\"\n"
        "}\n",
        r->verdict, verdict, notes) > 0;

    if (!ok) {
        fclose(f);
        remove(tmp);
        return AS_ERR_IO;
    }
    return atomic_commit(f, path, tmp);
}

int as_report_write_html(const AsReport *r, const char *path) {
    if (!r) return AS_ERR_INVALID;
    char tmp[1024];
    FILE *f = NULL;
    int rc = atomic_open(path, tmp, sizeof(tmp), &f);
    if (rc != AS_OK) return rc;

    char device[192], mode[64], calid[192], verdict[256], notes[384];
    html_escape(r->device, device, sizeof(device));
    html_escape(r->input_mode, mode, sizeof(mode));
    html_escape(r->calibration_id, calid, sizeof(calid));
    html_escape(r->verdict_text, verdict, sizeof(verdict));
    html_escape(r->notes, notes, sizeof(notes));
    const char *vclass = r->verdict == 0 ? "pass" : (r->verdict == 1 ? "warn" : "fail");

    int ok = fprintf(f,
        "<!doctype html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\">\n"
        "<title>%s — measurement report</title>\n"
        "<style>\n"
        ":root{--bg:#161718;--panel:#202224;--text:#e8e6e1;--dim:#a6a39c;--pass:#7fa37a;--warn:#d6a75c;--fail:#c96b63;}\n"
        "body{margin:0;background:var(--bg);color:var(--text);font:14px/1.5 -apple-system,Segoe UI,sans-serif;}\n"
        "main{max-width:860px;margin:0 auto;padding:32px;}\n"
        "h1{font-size:22px;letter-spacing:.02em;margin:0 0 4px;} .sub{color:var(--dim);margin:0 0 24px;}\n"
        ".verdict{display:inline-block;padding:6px 12px;border-radius:4px;font-weight:700;letter-spacing:.04em;}\n"
        ".pass{background:rgba(127,163,122,.18);color:var(--pass);} .warn{background:rgba(214,167,92,.18);color:var(--warn);} .fail{background:rgba(201,107,99,.18);color:var(--fail);}\n"
        "table{width:100%%;border-collapse:collapse;margin:16px 0 24px;background:var(--panel);}\n"
        "th,td{text-align:left;padding:8px 10px;border-bottom:1px solid #303335;font-variant-numeric:tabular-nums;}\n"
        "th{color:var(--dim);font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:.06em;}\n"
        "h2{font-size:14px;text-transform:uppercase;letter-spacing:.08em;color:var(--dim);margin:28px 0 4px;}\n"
        "code{color:var(--text);} .note{color:var(--dim);font-size:12px;margin-top:28px;}\n"
        "</style></head><body><main>\n",
        as_brand_string()) > 0;

    ok = ok && fprintf(f,
        "<h1>%s</h1><p class=\"sub\">measurement report · %s · %s</p>\n"
        "<div class=\"verdict %s\">%s</div>\n"
        "<h2>Run</h2><table>\n"
        "<tr><th>Run ID</th><td>%s</td></tr>\n"
        "<tr><th>Timestamp</th><td>%s</td></tr>\n"
        "<tr><th>Platform</th><td>%s</td></tr>\n"
        "<tr><th>Device</th><td>%s</td></tr>\n"
        "<tr><th>Input mode</th><td>%s</td></tr>\n"
        "<tr><th>Sample rate</th><td>%.0f Hz</td></tr>\n"
        "<tr><th>Frames</th><td>%d (%.3f s)</td></tr>\n"
        "<tr><th>Calibration</th><td>%s · %s · FS %.6f Vrms</td></tr>\n"
        "</table>\n",
        as_brand_string(), as_version_string(), r->timestamp, vclass, verdict,
        r->run_id, r->timestamp, r->platform, device, mode,
        r->sample_rate, r->frames, r->seconds,
        calid, as_confidence_name(r->confidence), r->fs_volts_rms) > 0;

    ok = ok && fprintf(f,
        "<h2>Channels</h2><table><tr><th>Channel</th><th>Peak dBFS</th>"
        "<th>RMS dBFS</th><th>Vpp</th><th>Vrms</th><th>Frequency</th>"
        "<th>Clip samples</th></tr>\n") > 0;
    for (int i = 0; ok && i < r->channels; ++i) {
        const AsChannelMeasurement *m = &r->ch[i];
        ok = fprintf(f,
            "<tr><td>%s</td><td>%.2f</td><td>%.2f</td><td>%.4f</td>"
            "<td>%.4f</td><td>%.2f Hz</td><td>%d</td></tr>\n",
            m->label, m->peak_dbfs, m->rms_dbfs, m->vpp, m->vrms,
            m->stats.freq, m->clip_count) > 0;
    }
    ok = ok && fprintf(f,
        "</table>\n<h2>Limits</h2><table>\n"
        "<tr><th>Frequency</th><td>%.1f – %.1f Hz</td></tr>\n"
        "<tr><th>Max Vpp</th><td>%s</td></tr>\n"
        "<tr><th>Min RMS</th><td>%s</td></tr>\n"
        "<tr><th>Max clip samples</th><td>%d</td></tr>\n"
        "<tr><th>Repeatability</th><td>%d runs · Vpp σ %.4f · freq σ %.2f Hz</td></tr>\n"
        "</table>\n",
        r->limits.min_freq_hz, r->limits.max_freq_hz,
        r->limits.max_vpp > 0.0 ? "configured" : "none",
        r->limits.min_rms > 0.0 ? "configured" : "none",
        r->limits.max_clip_samples, r->repeat_count, r->vpp_std, r->freq_std) > 0;
    ok = ok && fprintf(f,
        "<p class=\"note\">%s</p>\n"
        "<p class=\"note\">AudioScope by BerryStudio · generated locally. "
        "Software can warn and mute, but it cannot protect the sound card from "
        "over-voltage. Use a protected front end.</p>\n"
        "</main></body></html>\n",
        notes) > 0;

    if (!ok) {
        fclose(f);
        remove(tmp);
        return AS_ERR_IO;
    }
    return atomic_commit(f, path, tmp);
}

/* ------------------------------------------------------------------ */
/* PDF report (optional waveform / spectrum pages)                     */
/* ------------------------------------------------------------------ */

#define AS_PDF_CHART_MAX_POINTS 1024

static void pdf_footer(AsPdf *p, int page, int total) {
    char line[96];
    snprintf(line, sizeof(line), "Page %d of %d", page, total);
    as_pdf_text(p, 50, 40, 8, line);
}

static void dbfs_text(char *buf, size_t n, double db) {
    if (!(db > -199.0)) snprintf(buf, n, "-inf");
    else snprintf(buf, n, "%.2f", db);
}

static double wave_sample(const float *wave, int i) {
    double v = (double)wave[i];
    if (!(v > -1.0e30 && v < 1.0e30)) return 0.0; /* NaN/Inf -> 0 */
    return v;
}

static double wave_peak(const float *wave, int n) {
    double peak = 0.0;
    for (int i = 0; i < n; ++i) {
        double a = fabs(wave_sample(wave, i));
        if (a > peak) peak = a;
    }
    return peak;
}

static double wave_freq_hz(const float *wave, int n, double rate) {
    if (!wave || n < 4 || !(rate > 0.0)) return 0.0;
    int first = -1, last = -1, crossings = 0;
    for (int i = 1; i < n; ++i) {
        double prev = wave_sample(wave, i - 1), cur = wave_sample(wave, i);
        int up = prev <= 0.0 && cur > 0.0;
        int down = prev >= 0.0 && cur < 0.0;
        if (up || down) {
            if (first < 0) first = i;
            last = i;
            ++crossings;
        }
    }
    if (crossings < 2 || last <= first) return 0.0;
    double cycles = (double)(crossings - 1) / 2.0;
    double span = (double)(last - first) / rate;
    if (span <= 0.0) return 0.0;
    return cycles / span;
}

static double report_freq_hz(const AsReport *r) {
    if (r && r->channels > 0 && r->ch[0].stats.freq > 0.0) return r->ch[0].stats.freq;
    return 0.0;
}

/* Peak-preserving decimation keeps long waveforms readable and the PDF small.
 * Returns the number of points written (<= cap). */
static int wave_points(const float *wave, int n, double scale, double mid,
                       double x0, double x1, double *xs, double *ys, int cap) {
    if (n <= cap) {
        for (int i = 0; i < n; ++i) {
            xs[i] = x0 + (x1 - x0) * (double)i / (double)(n - 1);
            ys[i] = mid + wave_sample(wave, i) * scale;
        }
        return n;
    }
    int cols = cap / 2;
    int out = 0;
    for (int c = 0; c < cols; ++c) {
        long long a = (long long)c * n / cols;
        long long b = (long long)(c + 1) * n / cols;
        if (b <= a) b = a + 1;
        if (b > n) b = n;
        double lo = wave_sample(wave, (int)a), hi = lo;
        for (long long k = a + 1; k < b; ++k) {
            double v = wave_sample(wave, (int)k);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        double pos = (double)a + (double)(b - a - 1) * 0.5;
        double x = x0 + (x1 - x0) * pos / (double)(n - 1);
        xs[out] = x;
        ys[out] = mid + hi * scale;
        ++out;
        xs[out] = x;
        ys[out] = mid + lo * scale;
        ++out;
    }
    return out;
}

static double clamp_db(double db) {
    if (!(db > -100.0)) return -100.0;
    if (db > 0.0) return 0.0;
    return db;
}

static double spec_x(int bin, int bins, double x0, double x1) {
    if (bins <= 1) return x0;
    double t = log10(1.0 + (double)bin) / log10((double)bins);
    return x0 + (x1 - x0) * t;
}

/* Max-preserving decimation: log-ish bin layout, at most cap points. */
static int spec_points(const double *spec_db, int bins, double x0, double x1,
                       double y0, double y1, double *xs, double *ys, int cap) {
    int out = 0;
    if (bins <= cap) {
        for (int i = 0; i < bins; ++i) {
            xs[out] = spec_x(i, bins, x0, x1);
            ys[out] = y0 + (y1 - y0) * (clamp_db(spec_db[i]) + 100.0) / 100.0;
            ++out;
        }
        return out;
    }
    for (int c = 0; c < cap; ++c) {
        long long a = (long long)c * bins / cap;
        long long b = (long long)(c + 1) * bins / cap;
        if (b <= a) b = a + 1;
        if (b > bins) b = bins;
        double best = clamp_db(spec_db[a]);
        for (long long k = a + 1; k < b; ++k) {
            double v = clamp_db(spec_db[k]);
            if (v > best) best = v;
        }
        int center = (int)((a + b - 1) / 2);
        xs[out] = spec_x(center, bins, x0, x1);
        ys[out] = y0 + (y1 - y0) * (best + 100.0) / 100.0;
        ++out;
    }
    return out;
}

static void pdf_draw_wave(AsPdf *p, const AsReport *r, const float *wave, int n,
                          int page, int total) {
    double peak = wave_peak(wave, n);
    double peak_db = peak > 0.0 ? 20.0 * log10(peak) : -200.0;
    if (peak_db < -200.0) peak_db = -200.0;
    char pdb[24];
    dbfs_text(pdb, sizeof(pdb), peak_db);
    double vpp = (r->channels > 0 && r->ch[0].vpp > 0.0) ? r->ch[0].vpp : 2.0 * peak;
    double freq = report_freq_hz(r);
    if (!(freq > 0.0)) freq = wave_freq_hz(wave, n, r->sample_rate);

    char line[256];
    as_pdf_text(p, 50, 790, 18, "Waveform");
    if (freq > 0.0)
        snprintf(line, sizeof(line), "Peak %s dBFS   Vpp %.4f   Freq %.1f Hz",
                 pdb, vpp, freq);
    else
        snprintf(line, sizeof(line), "Peak %s dBFS   Vpp %.4f   Freq n/a",
                 pdb, vpp);
    as_pdf_text(p, 50, 766, 9, line);

    const double x0 = 85.0, x1 = 535.0, y0 = 500.0, y1 = 740.0;
    const double mid = (y0 + y1) * 0.5;
    const double amp = (y1 - y0) * 0.5;

    as_pdf_gray(p, 205, 0.3);
    for (int i = 1; i < 8; ++i) {
        double x = x0 + (x1 - x0) * (double)i / 8.0;
        as_pdf_line(p, x, y0, x, y1);
    }
    for (int i = 1; i < 4; ++i) {
        double y = y0 + (y1 - y0) * (double)i / 4.0;
        as_pdf_line(p, x0, y, x1, y);
    }

    as_pdf_gray(p, 90, 0.8);
    as_pdf_line(p, x0, y0, x1, y0);
    as_pdf_line(p, x1, y0, x1, y1);
    as_pdf_line(p, x1, y1, x0, y1);
    as_pdf_line(p, x0, y1, x0, y0);

    char v[32];
    snprintf(v, sizeof(v), "+%.3f", peak);
    as_pdf_text(p, x0 - 44, y1 - 2, 7, v);
    as_pdf_text(p, x0 - 44, mid - 2, 7, "0");
    snprintf(v, sizeof(v), "-%.3f", peak);
    as_pdf_text(p, x0 - 44, y0 - 2, 7, v);
    as_pdf_text(p, x0, y0 - 14, 7, "0");
    int shown = n > AS_PDF_CHART_MAX_POINTS ? AS_PDF_CHART_MAX_POINTS : n;
    if (shown < n)
        snprintf(v, sizeof(v), "%d of %d samples", shown, n);
    else
        snprintf(v, sizeof(v), "%d samples", n);
    as_pdf_text(p, x1 - 92, y0 - 14, 7, v);

    double scale = peak > 0.0 ? (amp * 0.9) / peak : 0.0;
    double xs[AS_PDF_CHART_MAX_POINTS], ys[AS_PDF_CHART_MAX_POINTS];
    int count = wave_points(wave, shown, scale, mid, x0, x1, xs, ys,
                            AS_PDF_CHART_MAX_POINTS);
    as_pdf_polyline(p, xs, ys, count, 45, 0.9);
    pdf_footer(p, page, total);
}

static void pdf_draw_spectrum(AsPdf *p, const double *spec_db, int bins,
                              int page, int total) {
    char line[256];
    as_pdf_text(p, 50, 790, 18, "Spectrum");
    snprintf(line, sizeof(line), "Bins %d   dBFS grid -100..0", bins);
    as_pdf_text(p, 50, 766, 9, line);

    const double x0 = 85.0, x1 = 535.0, y0 = 500.0, y1 = 740.0;

    as_pdf_gray(p, 205, 0.3);
    for (int i = 0; i <= 10; ++i) {
        double y = y0 + (y1 - y0) * (double)i / 10.0;
        as_pdf_line(p, x0, y, x1, y);
    }
    for (int i = 1; i < 8; ++i) {
        double x = x0 + (x1 - x0) * (double)i / 8.0;
        as_pdf_line(p, x, y0, x, y1);
    }

    as_pdf_gray(p, 90, 0.8);
    as_pdf_line(p, x0, y0, x1, y0);
    as_pdf_line(p, x1, y0, x1, y1);
    as_pdf_line(p, x1, y1, x0, y1);
    as_pdf_line(p, x0, y1, x0, y0);

    char v[32];
    for (int db = -100; db <= 0; db += 20) {
        double y = y0 + (y1 - y0) * (double)(db + 100) / 100.0;
        snprintf(v, sizeof(v), "%d", db);
        as_pdf_text(p, x0 - 26, y - 2, 7, v);
    }
    for (int k = 0; k <= 4; ++k) {
        double t = (double)k / 4.0;
        double x = x0 + (x1 - x0) * t;
        int bin = (int)(pow((double)bins, t) - 1.0 + 0.5);
        if (bin < 0) bin = 0;
        if (bin >= bins) bin = bins - 1;
        snprintf(v, sizeof(v), "%d", bin);
        as_pdf_text(p, x - 4, y0 - 14, 7, v);
    }

    double xs[AS_PDF_CHART_MAX_POINTS], ys[AS_PDF_CHART_MAX_POINTS];
    int count = spec_points(spec_db, bins, x0, x1, y0, y1, xs, ys,
                            AS_PDF_CHART_MAX_POINTS);
    as_pdf_polyline(p, xs, ys, count, 45, 0.9);
    pdf_footer(p, page, total);
}

int as_report_write_pdf_ex(const AsReport *r, const float *wave, int wave_n,
                           const double *spec_db, int spec_bins, const char *path) {
    if (!r || !path || !path[0]) return AS_ERR_INVALID;
    int wave_page = (wave && wave_n > 1);
    int spec_page = (spec_db && spec_bins > 1);
    int total = 1 + wave_page + spec_page;

    AsPdf *p = as_pdf_create("AudioScope report");
    if (!p) return AS_ERR_IO;
    char line[256];
    as_pdf_text(p, 50, 790, 18, AS_BRAND);
    as_pdf_text(p, 50, 768, 10, "Measurement report");
    as_pdf_line(p, 50, 758, 545, 758);
    snprintf(line, sizeof(line), "Run: %s   Time: %s", r->run_id, r->timestamp);
    as_pdf_text(p, 50, 742, 9, line);
    snprintf(line, sizeof(line), "Platform: %s   Device: %s", r->platform, r->device);
    as_pdf_text(p, 50, 728, 9, line);
    snprintf(line, sizeof(line), "Mode: %s   Rate: %.0f Hz   Frames: %d   %.2f s",
             r->input_mode, r->sample_rate, r->frames, r->seconds);
    as_pdf_text(p, 50, 714, 9, line);
    snprintf(line, sizeof(line), "Calibration: %s   Confidence: %s",
             r->calibration_id, as_confidence_name(r->confidence));
    as_pdf_text(p, 50, 700, 9, line);
    snprintf(line, sizeof(line), "Verdict: %s", r->verdict_text);
    as_pdf_text(p, 50, 680, 12, line);
    as_pdf_line(p, 50, 670, 545, 670);
    as_pdf_text(p, 50, 650, 9, "CH   Peak dBFS   RMS dBFS   Freq Hz   Vpp V   Clip");
    double y = 634;
    for (int i = 0; i < r->channels && i < AS_MAX_REPORT_CH; ++i) {
        const AsChannelMeasurement *m = &r->ch[i];
        snprintf(line, sizeof(line), "%-4s %9.1f %10.1f %9.1f %8.4f %5d",
                 m->label, m->peak_dbfs, m->rms_dbfs, m->stats.freq,
                 m->vpp, m->clip_count);
        as_pdf_text(p, 50, y, 9, line);
        y -= 15.0;
    }
    as_pdf_line(p, 50, y + 4, 545, y + 4);
    y -= 12;
    snprintf(line, sizeof(line), "Limits: max Vpp %.3f   min RMS %.1f dBFS   clip<=%d   %.0f-%.0f Hz",
             r->limits.max_vpp, r->limits.min_rms, r->limits.max_clip_samples,
             r->limits.min_freq_hz, r->limits.max_freq_hz);
    as_pdf_text(p, 50, y, 8, line);
    y -= 14;
    snprintf(line, sizeof(line), "Repeat: %d   Vpp sigma: %.5f   Freq sigma: %.3f Hz",
             r->repeat_count, r->vpp_std, r->freq_std);
    as_pdf_text(p, 50, y, 8, line);
    y -= 24;
    as_pdf_text(p, 50, y, 8,
                "Software cannot protect the sound card. Use a protected front end for high voltage.");
    as_pdf_text(p, 50, y - 14, 8,
                "Canonical report: self-contained HTML. This PDF is a printable summary.");
    as_pdf_text(p, 50, 60, 8, AS_BRAND " " AS_VERSION);
    pdf_footer(p, 1, total);

    if (wave_page) {
        if (as_pdf_new_page(p) != 0) {
            as_pdf_destroy(p);
            return AS_ERR_IO;
        }
        pdf_draw_wave(p, r, wave, wave_n, 2, total);
    }
    if (spec_page) {
        if (as_pdf_new_page(p) != 0) {
            as_pdf_destroy(p);
            return AS_ERR_IO;
        }
        pdf_draw_spectrum(p, spec_db, spec_bins, wave_page ? 3 : 2, total);
    }

    char tmp[1024];
    if (strlen(path) + 5 >= sizeof(tmp)) {
        as_pdf_destroy(p);
        return AS_ERR_INVALID;
    }
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int rc = as_pdf_finish(p, tmp);
    as_pdf_destroy(p);
    if (rc != 0) {
        remove(tmp);
        return AS_ERR_IO;
    }
    remove(path);
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return AS_ERR_IO;
    }
    return AS_OK;
}

int as_report_write_pdf(const AsReport *r, const char *path) {
    return as_report_write_pdf_ex(r, NULL, 0, NULL, 0, path);
}
