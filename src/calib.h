/* AudioScope 1.1 — 校准数据（声卡自身的频响/延迟/幅度误差）。 */
#ifndef AS_CALIB_H
#define AS_CALIB_H

#define CAL_MAX 64

typedef struct {
    int valid;
    int n;
    double f[CAL_MAX];
    double gain_db[CAL_MAX];    /* 直连回环实测误差（相对 0 dB） */
    double phase_deg[CAL_MAX];
    double delay_ms;            /* 输出→输入 往返延迟 */
    double amp_err_db;          /* 1 kHz 处幅度误差 */
    double fs;
} CalibData;

void   calib_clear(CalibData *c);
double calib_gain_at(const CalibData *c, double f);
double calib_phase_at(const CalibData *c, double f);
int    calib_save(const CalibData *c, const char *path);
int    calib_load(CalibData *c, const char *path);

#endif
