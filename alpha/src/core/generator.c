#include "core/generator.h"

#include <math.h>
#include <string.h>

#define AS_PI 3.14159265358979323846
#define AS_TWO_PI (2.0 * AS_PI)

static const char *WAVE_NAME[AS_WAVE_COUNT] = {
    "正弦", "方波", "三角", "锯齿", "白噪",
    "粉噪", "对数扫频", "单位脉冲", "阶跃"};
static const char *WAVE_ID[AS_WAVE_COUNT] = {
    "sine", "square", "triangle", "saw", "white",
    "pink", "logsweep", "impulse", "step"};

const char *as_wave_name(int wave) {
    if (wave < 0 || wave >= AS_WAVE_COUNT) return WAVE_NAME[0];
    return WAVE_NAME[wave];
}

const char *as_wave_id(int wave) {
    if (wave < 0 || wave >= AS_WAVE_COUNT) return WAVE_ID[0];
    return WAVE_ID[wave];
}

int as_wave_parse(const char *id) {
    if (!id) return AS_WAVE_SINE;
    for (int i = 0; i < AS_WAVE_COUNT; ++i)
        if (strcmp(id, WAVE_ID[i]) == 0) return i;
    return AS_WAVE_SINE;
}

int as_wave_is_periodic_pulse(int wave) {
    return wave == AS_WAVE_IMPULSE || wave == AS_WAVE_STEP;
}

void as_generator_reset(AsGenerator *gen, double fs) {
    if (!gen) return;
    memset(gen, 0, sizeof(*gen));
    gen->fs = fs > 0.0 ? fs : 48000.0;
    gen->rng = 0x9e3779b9u;  /* 固定种子：同参数逐位可复现 */
}

void as_generator_init(AsGenerator *gen, double fs) {
    as_generator_reset(gen, fs);
}

/* PolyBLEP：在阶跃处加一段二次修正，把方波/锯齿的能量从混叠搬回基带。
 * t 是周期内位置 0..1，dt 是每样本相位增量。 */
static double poly_blep(double t, double dt) {
    if (dt <= 0.0) return 0.0;
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

static double white_sample(AsGenerator *gen) {
    uint32_t x = gen->rng ? gen->rng : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    gen->rng = x;
    return (double)x * (2.0 / 4294967295.0) - 1.0;
}

/* Paul Kellet 的粉噪近似：7 个一阶低通叠加。 */
static double pink_sample(AsGenerator *gen) {
    const double w = white_sample(gen);
    gen->pink[0] = 0.99886 * gen->pink[0] + w * 0.0555179;
    gen->pink[1] = 0.99332 * gen->pink[1] + w * 0.0750759;
    gen->pink[2] = 0.96900 * gen->pink[2] + w * 0.1538520;
    gen->pink[3] = 0.86650 * gen->pink[3] + w * 0.3104856;
    gen->pink[4] = 0.55000 * gen->pink[4] + w * 0.5329522;
    gen->pink[5] = -0.7616 * gen->pink[5] - w * 0.0168980;
    const double v = gen->pink[0] + gen->pink[1] + gen->pink[2] +
                     gen->pink[3] + gen->pink[4] + gen->pink[5] +
                     gen->pink[6] + w * 0.5362;
    gen->pink[6] = w * 0.115926;
    return v * 0.11;  /* 归一到 ~±1 */
}


void as_gen_render(AsGenerator *gen, const AsGenConfig *cfg, float *out,
                   int frames) {
    if (!gen || !cfg || !out || frames <= 0) return;
    int wave = cfg->wave;
    if (wave < 0 || wave >= AS_WAVE_COUNT) wave = AS_WAVE_SINE;

    double amp = cfg->amp;
    if (!(amp >= 0.0)) amp = 0.0;
    if (amp > 1.0) amp = 1.0;

    const double fs = gen->fs > 0.0 ? gen->fs : 48000.0;
    double freq = cfg->freq_hz;
    if (!(freq > 0.1)) freq = 1000.0;
    if (freq > fs * 0.45) freq = fs * 0.45;

    double f_end = cfg->freq_end_hz;
    if (!(f_end > freq)) f_end = freq * 2.0;
    if (f_end > fs * 0.45) f_end = fs * 0.45;

    const double period = cfg->period_s > 0.0 ? cfg->period_s : 1.0;
    const double step = AS_TWO_PI * freq / fs;
    const double dt = freq / fs;
    const double sweep_ln = log(f_end / freq);
    const double sweep_inc = 1.0 / (fs * period);
    int64_t period_samples = (int64_t)(fs * period + 0.5);
    if (period_samples < 2) period_samples = 2;

    for (int i = 0; i < frames; ++i) {
        double v = 0.0;
        switch (wave) {
        case AS_WAVE_SINE:
            v = sin(gen->phase);
            break;
        case AS_WAVE_SQUARE: {
            const double t = gen->phase / AS_TWO_PI;
            v = t < 0.5 ? 1.0 : -1.0;
            v += poly_blep(t, dt);
            double t2 = t + 0.5;
            if (t2 >= 1.0) t2 -= 1.0;
            v -= poly_blep(t2, dt);
            break;
        }
        case AS_WAVE_TRIANGLE: {
            const double t = gen->phase / AS_TWO_PI;
            v = 4.0 * fabs(t - 0.5) - 1.0;
            break;
        }
        case AS_WAVE_SAW: {
            const double t = gen->phase / AS_TWO_PI;
            v = 2.0 * t - 1.0 - poly_blep(t, dt);
            break;
        }
        case AS_WAVE_WHITE:
            v = white_sample(gen);
            break;
        case AS_WAVE_PINK:
            v = pink_sample(gen);
            break;
        case AS_WAVE_LOG_SWEEP: {
            const double f = freq * exp(sweep_ln * gen->sweep_pos);
            v = sin(gen->phase);
            gen->phase += AS_TWO_PI * f / fs;
            if (gen->phase >= AS_TWO_PI) gen->phase -= AS_TWO_PI;
            gen->sweep_pos += sweep_inc;
            if (gen->sweep_pos >= 1.0) gen->sweep_pos = 0.0;
            out[i] = (float)(amp * v);
            continue;  /* 相位已在上面推进 */
        }
        case AS_WAVE_IMPULSE:
            v = (gen->tick == 0) ? 1.0 : 0.0;
            break;
        case AS_WAVE_STEP:
            v = (gen->tick >= period_samples / 2) ? 1.0 : 0.0;
            break;
        default:
            v = sin(gen->phase);
            break;
        }
        out[i] = (float)(amp * v);

        if (as_wave_is_periodic_pulse(wave)) {
            if (++gen->tick >= period_samples) gen->tick = 0;
        } else {
            gen->phase += step;
            if (gen->phase >= AS_TWO_PI) gen->phase -= AS_TWO_PI;
        }
    }
}

void as_generator_sine(AsGenerator *gen, double freq, double amp,
                       float *out, int frames) {
    AsGenConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.wave = AS_WAVE_SINE;
    cfg.freq_hz = freq;
    cfg.freq_end_hz = freq;
    cfg.amp = amp;
    cfg.period_s = 1.0;
    as_gen_render(gen, &cfg, out, frames);
}

void as_generator_square(AsGenerator *gen, double freq, double amp,
                         float *out, int frames) {
    AsGenConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.wave = AS_WAVE_SQUARE;
    cfg.freq_hz = freq;
    cfg.freq_end_hz = freq;
    cfg.amp = amp;
    cfg.period_s = 1.0;
    as_gen_render(gen, &cfg, out, frames);
}
