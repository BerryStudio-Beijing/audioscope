/* AudioScope 1.1 — 确定性测试信号发生器（自写算法，无查表/随机依赖）。
 *
 * 设计要点：
 *  - 相位累加器 + 双精度，频率由 f1/f2 与 sweep_sec 决定，不做任何波形点拼接；
 *  - 方波/锯齿用 PolyBLEP 抑制混叠，三角波是带限方波的积分（O(1)/样本）；
 *  - 白噪/粉噪用固定种子的 xorshift64*，同参数必然得到同一序列；
 *  - 所有参数显式，wg_check() 可量化频率误差(ppm)、幅度误差(dB)、THD、非谐波杂散。
 */
#ifndef AS_WAVEGEN_H
#define AS_WAVEGEN_H

#include <stdint.h>

enum {
    WG_SINE = 0,
    WG_SQUARE,
    WG_TRIANGLE,
    WG_SAW,
    WG_LINSWEEP,
    WG_LOGSWEEP,
    WG_MULTITONE,
    WG_WHITE,
    WG_PINK,
    WG_COUNT
};

#define WG_MAX_TONES 8

typedef struct {
    int wave;
    double fs;
    double f1, f2;        /* Hz；单频时 f1 == f2 */
    double amp;           /* 峰值（1.0 = 数字满刻度） */
    double phase0;        /* 起始相位 0..1（周期归一） */
    double sweep_sec;     /* 扫频时长，秒 */
    int blep;             /* 1 = 带限（默认） */
    int ntones;           /* 多频点个数 */
    double tones[WG_MAX_TONES];
    double tone_rel[WG_MAX_TONES];
    uint64_t seed;        /* 噪声种子（固定 → 可复现） */
} WgConfig;

typedef struct {
    WgConfig cfg;
    double phase;         /* 0..1 累加器 */
    double t;             /* 扫频累计秒 */
    double integ;         /* 三角波积分器 */
    uint64_t rng;
    double pink[7];
    uint64_t samples;
} WgState;

void   wg_default(WgConfig *c, double fs);
void   wg_init(WgState *s, const WgConfig *c);
double wg_next(WgState *s);
void   wg_render(WgState *s, float *dst, int frames);
/* 单一波形核心：任一处生成器都调用它，保证算法只有一份实现 */
double wg_shape(int wave, double phase, double inc, int blep);

/* 自检结果：用于单元测试与“正弦波频率/幅度误差可量化”的要求 */
typedef struct {
    double freq_req, freq_meas, freq_err_ppm;
    double amp_req_db, amp_meas_db, amp_err_db;
    double thd_percent;
    double spur_dbc;      /* 最强非谐波分量，相对基波 */
    double dc;
    int clipped;
    int settled;          /* 频率是否可测（扫频/噪声时为 0） */
} WgCheck;

/* 渲染 frames 个样本并用 FFT/时域指标量化，返回 0 成功 */
int wg_check(const WgConfig *c, int frames, WgCheck *out);

const char *wg_name(int wave);

#endif
