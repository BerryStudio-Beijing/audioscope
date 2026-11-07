/* AudioScope by BerryStudio 0.7-alpha — measurement report (JSON + HTML). */
#ifndef AS_CORE_REPORT_H
#define AS_CORE_REPORT_H

#include "core/api.h"
#include "core/stats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double min_freq_hz;
    double max_freq_hz;
    double max_vpp;
    double min_rms;
    int max_clip_samples;
} AsLimits;

typedef struct {
    char label[16];
    int channel;
    AsStats stats;
    double peak_dbfs;
    double rms_dbfs;
    double vrms;
    double vpp;
    int clip_count;
} AsChannelMeasurement;

#define AS_MAX_REPORT_CH 8

typedef struct {
    int schema_version;
    char run_id[64];
    char timestamp[32];
    char platform[32];
    char device[96];
    char input_mode[24];
    double sample_rate;
    int frames;
    double seconds;
    AsConfidence confidence;
    char calibration_id[96];
    double fs_volts_rms;
    AsLimits limits;
    int channels;
    AsChannelMeasurement ch[AS_MAX_REPORT_CH];
    int repeat_count;
    double vpp_std;
    double freq_std;
    int verdict; /* 0 pass, 1 warn, 2 fail */
    char verdict_text[192];
    char notes[256];
} AsReport;

void as_limits_default(AsLimits *limits);
void as_report_init(AsReport *report, const char *device, const char *input_mode,
                    double sample_rate, int frames, double seconds);
int  as_report_evaluate(AsReport *report);
int  as_report_write_json(const AsReport *report, const char *path);
int  as_report_write_html(const AsReport *report, const char *path);
int  as_report_write_pdf(const AsReport *report, const char *path);
const char *as_verdict_name(int verdict);

#ifdef __cplusplus
}
#endif

#endif
