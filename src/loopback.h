/* AudioScope 1.1 — 本地闭环测试流程（纯测量，不依赖任何在线服务）。
 *
 * 链路：声卡输出 → 被测效果器/DI → 声卡输入。
 * 流程：延迟 → 频响(可含校准) → THD+N → 噪声底 → 上升沿 → 报告。
 * 每个 tick 只推进一步，UI 不会卡住；同样参数重复运行结果一致。
 */
#ifndef AS_LOOPBACK_H
#define AS_LOOPBACK_H

#include <stdint.h>

#include "analyze.h"
#include "audio.h"
#include "calib.h"
#include "report.h"

enum { LB_IDLE = 0, LB_RUN, LB_DONE, LB_FAIL };

typedef struct {
    double f1, f2;
    int points;
    double level_dbfs;
    double tol_db;
    int do_delay, do_fr, do_cal, do_thdn, do_noise, do_step;
} LbConfig;

typedef struct {
    LbConfig cfg;
    int state;
    int step;
    int phase;
    double wait_until;
    double progress;
    char stage[96];
    char error[128];

    double freqs[AN_MAX_FR];
    int nfreq, freq_idx;
    FrPoint fr[AN_MAX_FR];
    int nfr;

    CalibData cal;         /* 本次直连测得的校准 */
    CalibData cal_store;   /* 已保存的校准（由 App 传入） */

    AnResult res;
    Report rep;
    double delay_ms;

    float *cap;
    int cap_alloc;
    int cap_n;

    double fs;
    double out_start;      /* 激励第 0 个样本对应的输出帧号 */
} Loopback;

void lb_default(LbConfig *c);
int  lb_begin(Loopback *lb, const LbConfig *cfg, double fs,
              const CalibData *saved_cal);
int  lb_poll(Loopback *lb, ASEngine *eng, GenCfg *gen, double now);
void lb_cancel(Loopback *lb);
const char *lb_step_name(int step);

#endif
