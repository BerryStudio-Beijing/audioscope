/* AudioScope by BerryStudio 0.5-alpha — null backend for platforms without an audio layer. */
#include "platform/audio_backend.h"
#include "core/ring.h"

#include <stdlib.h>
#include <string.h>

struct AsAudio {
    AsRing ring;
    double fs;
    int running;
};

static char g_error[256];

const char *as_audio_last_error(void) {
    return g_error;
}

AsAudio *as_audio_open(int in_index, int out_index, double fs, int block) {
    (void)in_index; (void)out_index; (void)block;
    memset(g_error, 0, sizeof(g_error));
    AsAudio *a = (AsAudio *)calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->fs = fs;
    if (as_ring_init(&a->ring, (uint32_t)(fs * 2.0), 2) != 0) {
        free(a);
        return NULL;
    }
    a->running = 1;
    return a;
}

void as_audio_close(AsAudio *a) {
    if (!a) return;
    as_ring_free(&a->ring);
    free(a);
}

int as_audio_read(AsAudio *a, float *dst, int frames) {
    if (!a || !dst || frames <= 0) return 0;
    memset(dst, 0, (size_t)frames * 2 * sizeof(float));
    return frames;
}

int as_audio_set_tone(AsAudio *a, double freq, double amp, int on) {
    (void)a; (void)freq; (void)amp; (void)on;
    return 0;
}

int as_audio_channels(const AsAudio *a) {
    (void)a;
    return 2;
}

double as_audio_rate(const AsAudio *a) {
    return a ? a->fs : 0.0;
}

int as_audio_running(const AsAudio *a) {
    return a ? a->running : 0;
}
