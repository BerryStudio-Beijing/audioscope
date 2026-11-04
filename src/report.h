/* AudioScope 1.1 — 报告（JSON / CSV / WAV）与电路调整建议。 */
#ifndef AS_REPORT_H
#define AS_REPORT_H

#include <stdint.h>

#include "analyze.h"

typedef struct {
    char in_dev[96], out_dev[96];
    double fs;
    int block, channels;
    double level_dbfs;
    int points;
    double f1, f2;
    int use_calib;
    double cal_delay_ms;
    uint32_t settings_hash;
    double tol_db;

    AnResult res;
    int nfr;
    FrPoint fr[AN_MAX_FR];

    int verdict;
    char verdict_text[160];
    char advice[12][160];
    int nadvice;
} Report;

int  report_json(const Report *r, const char *path);
int  report_csv(const Report *r, const char *path);
int  report_wav24(const float *x, int n, double fs, const char *path);
/* 规则式建议（不调用任何外部服务）；同时写入 r->advice / verdict */
void report_build(Report *r, const CalibData *cal);

#endif
