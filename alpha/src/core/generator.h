/* AudioScope by BerryStudio 0.9.3-beta — signal generator (mono float).
 *
 * 实时安全：`as_gen_render()` 不分配、不加锁、不做 IO，可在音频回调里直接调用。
 * 波形集面向维修测量：正弦 / 方波 / 三角 / 锯齿 / 白噪 / 粉噪 / 对数扫频 /
 * 单位脉冲 / 阶跃。方波与锯齿用 PolyBLEP 抑制混叠；噪声用固定种子 xorshift，
 * 同参数逐位可复现。
 */
#ifndef AS_CORE_GENERATOR_H
#define AS_CORE_GENERATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AS_WAVE_SINE = 0,
    AS_WAVE_SQUARE,
    AS_WAVE_TRIANGLE,
    AS_WAVE_SAW,
    AS_WAVE_WHITE,
    AS_WAVE_PINK,
    AS_WAVE_LOG_SWEEP,
    AS_WAVE_IMPULSE,
    AS_WAVE_STEP,
    AS_WAVE_COUNT
} AsWaveform;

typedef struct {
    double fs;
    double phase;      /* 弧度 0..2π */
    double sweep_pos;  /* 对数扫频位置 0..1 */
    double pink[7];    /* 粉噪滤波器状态 */
    uint32_t rng;      /* xorshift32，固定种子可复现 */
    int64_t tick;      /* 当前周期内已输出样本数（脉冲/阶跃用） */
} AsGenerator;

typedef struct {
    int wave;          /* AsWaveform */
    double freq_hz;    /* 起始频率（扫频）或基频 */
    double freq_end_hz;/* 对数扫频终止频率（其它波形忽略） */
    double amp;        /* 线性峰值 0..1 */
    double period_s;   /* 扫频 / 脉冲 / 阶跃周期（秒） */
} AsGenConfig;

const char *as_wave_name(int wave);  /* 中文短名，用于界面 */
const char *as_wave_id(int wave);    /* ASCII id，用于 CLI / 配置 */
int as_wave_parse(const char *id);   /* 未知返回 AS_WAVE_SINE */
int as_wave_is_periodic_pulse(int wave); /* 脉冲 / 阶跃：频率栏表示周期 */

void as_generator_init(AsGenerator *gen, double fs);
void as_generator_reset(AsGenerator *gen, double fs);
void as_gen_render(AsGenerator *gen, const AsGenConfig *cfg, float *out,
                   int frames);

/* 兼容旧调用（0.1-alpha 起就在用）：纯正弦 / 方波。 */
void as_generator_sine(AsGenerator *gen, double freq, double amp,
                       float *out, int frames);
void as_generator_square(AsGenerator *gen, double freq, double amp,
                         float *out, int frames);

#ifdef __cplusplus
}
#endif

#endif
