/* AudioScope 1.1 — 采集分析：延迟对齐、同步平均、频响/相位/失真/噪声/上升沿。 */
#ifndef AS_ANALYZE_H
#define AS_ANALYZE_H

#include <stdint.h>

#include "calib.h"

#define AN_MAX_FR 64

typedef struct {
    double f;
    double gain_db;      /* 已扣除校准 */
    double gain_raw_db;  /* 未扣校准 */
    double phase_deg;
    double amp_err_db;   /* 实测激励幅度 - 目标 */
} FrPoint;

typedef struct {
    double flatness_db;    /* 频响峰谷差 */
    double max_dev_db;
    double max_dev_f;
    double amp_err_db;
    double thd_percent;
    double thdn_percent;
    double snr_db;
    double sinad_db;
    double noise_dbfs;
    double delay_ms;
    double rise_us;
    double phase_dev_deg;
    int clip_count;
    int verdict;           /* 0 = 未通过, 1 = 通过 */
    int have_fr, have_thd, have_noise, have_step, have_delay;
} AnResult;

/* 互相关延迟（采样点，可为小数）；peak 返回归一化相关峰 0..1 */
double an_xcorr_delay(const float *tx, int ntx, const float *rx, int nrx,
                      double *peak);
/* 单频点峰值幅度与相位（BH 窗 DFT） */
double an_tone(const float *x, int n, double fs, double f, double *phase_deg);
/* 同步平均：把 n 点按 period 折叠成一段平均（降低随机噪声） */
int an_sync_average(const float *x, int n, int period, float *out, int cap);
double an_noise_floor_dbfs(const float *x, int n);
double an_rise_time_us(const float *x, int n, double fs);
int an_fr_from_tones(const float *cap, int n, double fs, const double *freqs,
                     int nf, const double *stim_amp, FrPoint *out);
int an_evaluate(const FrPoint *fr, int nfr, const CalibData *cal, AnResult *r,
                double tol_db);

#endif
