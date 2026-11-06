/* AudioScope by BerryStudio 0.3-alpha — lock-free single-producer/single-consumer ring.
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
#include <atomic>
typedef std::atomic<uint32_t> AsAtomicU32;
extern "C" {
#elif defined(_MSC_VER)
#include <intrin.h>
typedef volatile uint32_t AsAtomicU32;
#else
#include <stdatomic.h>
typedef _Atomic uint32_t AsAtomicU32;
#endif

typedef struct {
    float *data;
    uint32_t cap;      /* frames, power of two */
    uint32_t mask;
    uint32_t channels;
    AsAtomicU32 write_frames;
} AsRing;

static inline uint32_t as_atomic_load_u32(const AsAtomicU32 *p) {
#ifdef __cplusplus
    return p->load(std::memory_order_acquire);
#elif defined(_MSC_VER)
    return *p;
#else
    return atomic_load_explicit(p, memory_order_acquire);
#endif
}

static inline void as_atomic_store_u32(AsAtomicU32 *p, uint32_t v) {
#ifdef __cplusplus
    p->store(v, std::memory_order_release);
#elif defined(_MSC_VER)
    _InterlockedExchange((volatile long *)p, (long)v);
#else
    atomic_store_explicit(p, v, memory_order_release);
#endif
}

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
