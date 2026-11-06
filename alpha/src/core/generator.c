#include "core/generator.h"

#include <math.h>

#define AS_PI 3.14159265358979323846

void as_generator_init(AsGenerator *gen, double fs) {
    if (!gen) return;
    gen->fs = fs > 0.0 ? fs : 48000.0;
    gen->phase = 0.0;
}

void as_generator_sine(AsGenerator *gen, double freq, double amp,
                       float *out, int frames) {
    if (!gen || !out || frames <= 0) return;
    double step = 2.0 * AS_PI * freq / gen->fs;
    for (int i = 0; i < frames; ++i) {
        out[i] = (float)(amp * sin(gen->phase));
        gen->phase += step;
        if (gen->phase >= 2.0 * AS_PI) gen->phase -= 2.0 * AS_PI;
    }
}

void as_generator_square(AsGenerator *gen, double freq, double amp,
                         float *out, int frames) {
    if (!gen || !out || frames <= 0) return;
    double step = 2.0 * AS_PI * freq / gen->fs;
    for (int i = 0; i < frames; ++i) {
        out[i] = (float)(amp * (sin(gen->phase) >= 0.0 ? 1.0 : -1.0));
        gen->phase += step;
        if (gen->phase >= 2.0 * AS_PI) gen->phase -= 2.0 * AS_PI;
    }
}
