/* AudioScope by BerryStudio 0.5-alpha — measurement report writer. */
#include "core/report.h"

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
