#include "core/ring.h"

#include <stdlib.h>
#include <string.h>

static uint32_t next_pow2(uint32_t v) {
    if (v < 2) return 2;
    --v;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

int as_ring_init(AsRing *ring, uint32_t capacity_frames, uint32_t channels) {
    if (!ring || capacity_frames == 0 || channels == 0) return -1;
    memset(ring, 0, sizeof(*ring));
    ring->cap = next_pow2(capacity_frames);
    ring->mask = ring->cap - 1;
    ring->channels = channels;
    ring->data = (float *)calloc((size_t)ring->cap * channels, sizeof(float));
    return ring->data ? 0 : -1;
}

void as_ring_reset(AsRing *ring) {
    if (!ring || !ring->data) return;
    memset(ring->data, 0, (size_t)ring->cap * ring->channels * sizeof(float));
    ring->write_frames = 0;
}

void as_ring_free(AsRing *ring) {
    if (!ring) return;
    free(ring->data);
    memset(ring, 0, sizeof(*ring));
}

void as_ring_write(AsRing *ring, const float *interleaved, int frames) {
    if (!ring || !ring->data || !interleaved || frames <= 0) return;
    uint32_t w = ring->write_frames;
    for (int i = 0; i < frames; ++i) {
        uint32_t slot = (w + (uint32_t)i) & ring->mask;
        for (uint32_t c = 0; c < ring->channels; ++c) {
            ring->data[(size_t)slot * ring->channels + c] =
                interleaved[(size_t)i * ring->channels + c];
        }
    }
    ring->write_frames = w + (uint32_t)frames;
}

int as_ring_read_latest(AsRing *ring, float *dst, int frames) {
    if (!ring || !ring->data || !dst || frames <= 0) return 0;
    uint32_t w = ring->write_frames;
    uint32_t available = w < ring->cap ? w : ring->cap;
    uint32_t take = (uint32_t)frames < available ? (uint32_t)frames : available;
    uint32_t start = w - take;
    for (uint32_t i = 0; i < take; ++i) {
        uint32_t slot = (start + i) & ring->mask;
        for (uint32_t c = 0; c < ring->channels; ++c) {
            dst[(size_t)i * ring->channels + c] =
                ring->data[(size_t)slot * ring->channels + c];
        }
    }
    return (int)take;
}

int as_ring_filled(const AsRing *ring) {
    if (!ring) return 0;
    uint32_t w = ring->write_frames;
    return (int)(w < ring->cap ? w : ring->cap);
}
