/* AudioScope by BerryStudio 0.2-alpha — lock-free single-producer/single-consumer ring.
 *
 * The audio callback writes interleaved stereo float frames; the UI reads the
 * most recent frames without advancing a consumer cursor (snapshot semantics).
 * The capacity is sized to several seconds, so a UI read is very unlikely to
 * race the writer by more than one wrap.
 */
#ifndef AS_CORE_RING_H
#define AS_CORE_RING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float *data;
    uint32_t cap;      /* frames, power of two */
    uint32_t mask;
    uint32_t channels;
    _Atomic uint32_t write_frames;
} AsRing;

int  as_ring_init(AsRing *ring, uint32_t capacity_frames, uint32_t channels);
void as_ring_reset(AsRing *ring);
void as_ring_free(AsRing *ring);
void as_ring_write(AsRing *ring, const float *interleaved, int frames);
int  as_ring_read_latest(AsRing *ring, float *dst, int frames);
int  as_ring_filled(const AsRing *ring);

#ifdef __cplusplus
}
#endif

#endif
