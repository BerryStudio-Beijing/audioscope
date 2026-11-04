#include "calib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void calib_clear(CalibData *c) { memset(c, 0, sizeof(*c)); }

/* 对数频率轴上做线性插值（超出范围取端点） */
double calib_gain_at(const CalibData *c, double f) {
    if (!c || !c->valid || c->n <= 0) return 0.0;
    if (c->n == 1) return c->gain_db[0];
    if (f <= c->f[0]) return c->gain_db[0];
    if (f >= c->f[c->n - 1]) return c->gain_db[c->n - 1];
    for (int i = 1; i < c->n; i++) {
        if (f <= c->f[i]) {
            double lf = log(f), l0 = log(c->f[i - 1]), l1 = log(c->f[i]);
            double t = (l1 > l0) ? (lf - l0) / (l1 - l0) : 0.0;
            return c->gain_db[i - 1] + (c->gain_db[i] - c->gain_db[i - 1]) * t;
        }
    }
    return c->gain_db[c->n - 1];
}

double calib_phase_at(const CalibData *c, double f) {
    if (!c || !c->valid || c->n <= 0) return 0.0;
    if (c->n == 1) return c->phase_deg[0];
    if (f <= c->f[0]) return c->phase_deg[0];
    if (f >= c->f[c->n - 1]) return c->phase_deg[c->n - 1];
    for (int i = 1; i < c->n; i++) {
        if (f <= c->f[i]) {
            double lf = log(f), l0 = log(c->f[i - 1]), l1 = log(c->f[i]);
            double t = (l1 > l0) ? (lf - l0) / (l1 - l0) : 0.0;
            return c->phase_deg[i - 1] +
                   (c->phase_deg[i] - c->phase_deg[i - 1]) * t;
        }
    }
    return c->phase_deg[c->n - 1];
}

int calib_save(const CalibData *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\n  \"type\": \"audioscope-calibration\",\n");
    fprintf(f, "  \"valid\": %d,\n  \"sample_rate\": %.0f,\n", c->valid, c->fs);
    fprintf(f, "  \"delay_ms\": %.6f,\n  \"amp_err_db\": %.4f,\n",
            c->delay_ms, c->amp_err_db);
    fprintf(f, "  \"points\": %d,\n  \"freq_hz\": [", c->n);
    for (int i = 0; i < c->n; i++)
        fprintf(f, "%s%.6g", i ? ", " : "", c->f[i]);
    fprintf(f, "],\n  \"gain_db\": [");
    for (int i = 0; i < c->n; i++)
        fprintf(f, "%s%.6f", i ? ", " : "", c->gain_db[i]);
    fprintf(f, "],\n  \"phase_deg\": [");
    for (int i = 0; i < c->n; i++)
        fprintf(f, "%s%.4f", i ? ", " : "", c->phase_deg[i]);
    fprintf(f, "]\n}\n");
    fclose(f);
    return 0;
}

static int grab(const char *s, const char *key, double *dst, int max) {
    const char *p = strstr(s, key);
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    p++;
    int n = 0;
    while (n < max) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
        if (*p == ']' || !*p) break;
        char *end = NULL;
        double v = strtod(p, &end);
        if (end == p) break;
        dst[n++] = v;
        p = end;
    }
    return n;
}

static double grab1(const char *s, const char *key, double def) {
    const char *p = strstr(s, key);
    if (!p) return def;
    p = strchr(p, ':');
    if (!p) return def;
    return atof(p + 1);
}

int calib_load(CalibData *c, const char *path) {
    calib_clear(c);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1 << 20) {
        fclose(f);
        return -1;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got] = 0;
    fclose(f);

    c->fs = grab1(buf, "\"sample_rate\"", 48000.0);
    c->delay_ms = grab1(buf, "\"delay_ms\"", 0.0);
    c->amp_err_db = grab1(buf, "\"amp_err_db\"", 0.0);
    int n = grab(buf, "\"freq_hz\"", c->f, CAL_MAX);
    int ng = grab(buf, "\"gain_db\"", c->gain_db, CAL_MAX);
    int np = grab(buf, "\"phase_deg\"", c->phase_deg, CAL_MAX);
    c->n = (n < ng) ? n : ng;
    if (c->n > np) c->n = np;
    c->valid = (c->n > 0);
    free(buf);
    return c->valid ? 0 : -1;
}
