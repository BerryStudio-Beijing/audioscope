/* AudioScope by BerryStudio 0.2-alpha — versioned calibration profiles. */
#include "core/api.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CAL_SCHEMA 2
#define CAL_MAX_JSON 8192

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

void as_calibration_init(AsCalibration *cal) {
    if (!cal) return;
    memset(cal, 0, sizeof(*cal));
    cal->schema_version = CAL_SCHEMA;
    cal->input_channel = 0;
    cal->gain_db = 0.0;
    cal->pad_db = 0.0;
    cal->sample_rate = 48000;
    cal->fs_volts_rms = 1.0;
    cal->input_impedance_ohm = 10000.0;
    cal->confidence = AS_CONF_ASSUMED;
    snprintf(cal->method, sizeof(cal->method), "%s", "manual");
    iso_now(cal->created_at, sizeof(cal->created_at));
}

static uint64_t fnv1a(uint64_t h, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

void as_calibration_seal(AsCalibration *cal) {
    if (!cal) return;
    char canon[512];
    snprintf(canon, sizeof(canon), "%s|%d|%.9g|%.9g|%d|%.9g|%.9g|%d|%s",
             cal->device_id, cal->input_channel, cal->gain_db, cal->pad_db,
             cal->sample_rate, cal->fs_volts_rms, cal->input_impedance_ohm,
             (int)cal->confidence, cal->method);
    uint64_t h = 1469598103934665603ULL;
    h = fnv1a(h, canon, strlen(canon));
    snprintf(cal->checksum, sizeof(cal->checksum), "%016llx",
             (unsigned long long)h);
}

const char *as_confidence_name(AsConfidence confidence) {
    switch (confidence) {
    case AS_CONF_MEASURED: return "measured";
    case AS_CONF_DATASHEET: return "datasheet";
    case AS_CONF_ASSUMED: return "assumed";
    default: return "assumed";
    }
}

static int confidence_from_name(const char *s, AsConfidence *out) {
    if (!s || !out) return 0;
    if (strcmp(s, "measured") == 0) { *out = AS_CONF_MEASURED; return 1; }
    if (strcmp(s, "datasheet") == 0) { *out = AS_CONF_DATASHEET; return 1; }
    if (strcmp(s, "assumed") == 0) { *out = AS_CONF_ASSUMED; return 1; }
    return 0;
}

int as_calibration_validate(const AsCalibration *cal, char *err, size_t err_len) {
    if (!cal) {
        if (err && err_len) snprintf(err, err_len, "null calibration");
        return AS_ERR_INVALID;
    }
    if (cal->schema_version != CAL_SCHEMA) {
        if (err && err_len) snprintf(err, err_len, "unsupported schema %d", cal->schema_version);
        return AS_ERR_INVALID;
    }
    if (cal->device_id[0] == '\0') {
        if (err && err_len) snprintf(err, err_len, "device_id is empty");
        return AS_ERR_INVALID;
    }
    if (cal->input_channel < 0 || cal->input_channel > 15) {
        if (err && err_len) snprintf(err, err_len, "input_channel out of range");
        return AS_ERR_INVALID;
    }
    if (cal->pad_db < -60.0 || cal->pad_db > 60.0 || !isfinite(cal->pad_db)) {
        if (err && err_len) snprintf(err, err_len, "pad_db out of range");
        return AS_ERR_INVALID;
    }
    if (cal->sample_rate < 8000 || cal->sample_rate > 384000) {
        if (err && err_len) snprintf(err, err_len, "sample_rate out of range");
        return AS_ERR_INVALID;
    }
    if (!(cal->fs_volts_rms > 0.0) || !isfinite(cal->fs_volts_rms) ||
        cal->fs_volts_rms > 1000.0) {
        if (err && err_len) snprintf(err, err_len, "fs_volts_rms out of range");
        return AS_ERR_INVALID;
    }
    if (cal->input_impedance_ohm < 0.0 || !isfinite(cal->input_impedance_ohm)) {
        if (err && err_len) snprintf(err, err_len, "input_impedance_ohm invalid");
        return AS_ERR_INVALID;
    }
    if (cal->confidence < AS_CONF_MEASURED || cal->confidence > AS_CONF_ASSUMED) {
        if (err && err_len) snprintf(err, err_len, "confidence invalid");
        return AS_ERR_INVALID;
    }
    if (cal->checksum[0] != '\0') {
        AsCalibration copy = *cal;
        char expected[80];
        snprintf(expected, sizeof(expected), "%s", copy.checksum);
        as_calibration_seal(&copy);
        if (strcmp(expected, copy.checksum) != 0) {
            if (err && err_len) snprintf(err, err_len, "checksum mismatch");
            return AS_ERR_INVALID;
        }
    }
    if (err && err_len) err[0] = '\0';
    return AS_OK;
}

double as_calibration_volts_rms(const AsCalibration *cal, double dbfs) {
    if (!cal || !(cal->fs_volts_rms > 0.0) || !isfinite(dbfs)) return 0.0;
    return cal->fs_volts_rms * pow(10.0, dbfs / 20.0);
}

/* ------------------------------------------------------------------ */
/* Minimal JSON helpers (fixed schema, no third-party parser).         */
/* ------------------------------------------------------------------ */

static const char *find_key(const char *json, const char *key) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p = strchr(p + strlen(pat), ':');
    if (!p) return NULL;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

static int json_string(const char *json, const char *key, char *out, size_t n) {
    const char *p = find_key(json, key);
    if (!p || *p != '"' || n == 0) return 0;
    ++p;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < n) {
        if (*p == '\\' && p[1]) {
            ++p;
            char c = *p;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            out[i++] = c;
            ++p;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return 1;
}

static int json_double(const char *json, const char *key, double *out) {
    const char *p = find_key(json, key);
    if (!p) return 0;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return 0;
    *out = v;
    return 1;
}

static int json_int(const char *json, const char *key, int *out) {
    const char *p = find_key(json, key);
    if (!p) return 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p) return 0;
    *out = (int)v;
    return 1;
}

int as_calibration_write_json(const AsCalibration *cal, const char *path) {
    if (!cal || !path || !path[0]) return AS_ERR_INVALID;
    AsCalibration copy = *cal;
    as_calibration_seal(&copy);
    char err[96];
    int rc = as_calibration_validate(&copy, err, sizeof(err));
    if (rc != AS_OK) return rc;

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return AS_ERR_IO;
    int ok = fprintf(f,
        "{\n"
        "  \"schema_version\": %d,\n"
        "  \"device_id\": \"%s\",\n"
        "  \"input_channel\": %d,\n"
        "  \"gain_db\": %.9g,\n"
        "  \"pad_db\": %.9g,\n"
        "  \"sample_rate\": %d,\n"
        "  \"fs_volts_rms\": %.9g,\n"
        "  \"input_impedance_ohm\": %.9g,\n"
        "  \"confidence\": \"%s\",\n"
        "  \"method\": \"%s\",\n"
        "  \"created_at\": \"%s\",\n"
        "  \"checksum\": \"%s\"\n"
        "}\n",
        copy.schema_version, copy.device_id, copy.input_channel,
        copy.gain_db, copy.pad_db, copy.sample_rate, copy.fs_volts_rms,
        copy.input_impedance_ohm,
        as_confidence_name(copy.confidence), copy.method,
        copy.created_at, copy.checksum) > 0;
    if (fclose(f) != 0) ok = 0;
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

int as_calibration_read_json(AsCalibration *cal, const char *path) {
    if (!cal || !path || !path[0]) return AS_ERR_INVALID;
    FILE *f = fopen(path, "rb");
    if (!f) return AS_ERR_IO;
    char buf[CAL_MAX_JSON];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    as_calibration_init(cal);
    int schema = CAL_SCHEMA;
    if (!json_int(buf, "schema_version", &schema)) return AS_ERR_INVALID;
    cal->schema_version = schema;
    if (!json_string(buf, "device_id", cal->device_id, sizeof(cal->device_id)))
        return AS_ERR_INVALID;
    if (!json_int(buf, "input_channel", &cal->input_channel))
        return AS_ERR_INVALID;
    if (!json_double(buf, "gain_db", &cal->gain_db))
        return AS_ERR_INVALID;
    if (!json_double(buf, "pad_db", &cal->pad_db))
        cal->pad_db = 0.0;
    if (!json_int(buf, "sample_rate", &cal->sample_rate))
        cal->sample_rate = 48000;
    if (!json_double(buf, "fs_volts_rms", &cal->fs_volts_rms))
        return AS_ERR_INVALID;
    if (!json_double(buf, "input_impedance_ohm", &cal->input_impedance_ohm))
        return AS_ERR_INVALID;

    char conf[32];
    if (!json_string(buf, "confidence", conf, sizeof(conf))) return AS_ERR_INVALID;
    if (!confidence_from_name(conf, &cal->confidence)) return AS_ERR_INVALID;
    if (!json_string(buf, "method", cal->method, sizeof(cal->method)))
        return AS_ERR_INVALID;
    if (!json_string(buf, "created_at", cal->created_at, sizeof(cal->created_at)))
        return AS_ERR_INVALID;
    if (!json_string(buf, "checksum", cal->checksum, sizeof(cal->checksum)))
        return AS_ERR_INVALID;

    char err[96];
    return as_calibration_validate(cal, err, sizeof(err));
}
