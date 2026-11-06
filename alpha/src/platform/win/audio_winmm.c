/* AudioScope by BerryStudio 0.2-alpha — Windows WinMM backend (2ch int16, 48 kHz).
 *
 * This is the alpha fallback. It is deliberately simple: 4 queued buffers,
 * int16 PCM, WAVE_MAPPER by default. WASAPI is the v1.2 target.
 */
#include "platform/audio_backend.h"
#include "core/api.h"
#include "core/ring.h"

#include <windows.h>
#include <mmsystem.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
typedef volatile LONG AsAtomicInt;
typedef volatile LONG64 AsAtomicU64;
static int as_atomic_load_int(const AsAtomicInt *p) { return (int)*p; }
static void as_atomic_store_int(AsAtomicInt *p, int v) { InterlockedExchange(p, (LONG)v); }
static uint64_t as_atomic_load_u64(const AsAtomicU64 *p) { return (uint64_t)*p; }
static void as_atomic_store_u64(AsAtomicU64 *p, uint64_t v) { InterlockedExchange64(p, (LONG64)v); }
#else
#include <stdatomic.h>
typedef _Atomic int AsAtomicInt;
typedef _Atomic uint64_t AsAtomicU64;
static int as_atomic_load_int(const AsAtomicInt *p) { return atomic_load_explicit(p, memory_order_relaxed); }
static void as_atomic_store_int(AsAtomicInt *p, int v) { atomic_store_explicit(p, v, memory_order_relaxed); }
static uint64_t as_atomic_load_u64(const AsAtomicU64 *p) { return atomic_load_explicit(p, memory_order_relaxed); }
static void as_atomic_store_u64(AsAtomicU64 *p, uint64_t v) { atomic_store_explicit(p, v, memory_order_relaxed); }
#endif

#define AS_WIN_BLOCK 512
#define AS_WIN_BUFFERS 4
#define AS_PI 3.14159265358979323846

struct AsAudio {
    HWAVEIN in;
    HWAVEOUT out;
    WAVEHDR in_hdr[AS_WIN_BUFFERS];
    WAVEHDR out_hdr[AS_WIN_BUFFERS];
    short in_buf[AS_WIN_BUFFERS][AS_WIN_BLOCK * 2];
    short out_buf[AS_WIN_BUFFERS][AS_WIN_BLOCK * 2];
    AsRing ring;
    double fs;
    int block;
    int running;
    AsAtomicInt tone_on;
    AsAtomicU64 freq_bits;
    AsAtomicU64 amp_bits;
    double phase;
};

static char g_error[256];

static void set_error(const char *text) {
    snprintf(g_error, sizeof(g_error), "%s", text ? text : "");
}

const char *as_audio_last_error(void) {
    return g_error;
}

static uint64_t double_bits(double v) {
    uint64_t u = 0;
    memcpy(&u, &v, sizeof(u));
    return u;
}

static double bits_double(uint64_t u) {
    double v = 0.0;
    memcpy(&v, &u, sizeof(v));
    return v;
}

static void CALLBACK in_callback(HWAVEIN h, UINT msg, DWORD_PTR user,
                                 DWORD_PTR p1, DWORD_PTR p2) {
    (void)p2;
    AsAudio *a = (AsAudio *)user;
    if (!a || msg != WIM_DATA || !p1) return;
    WAVEHDR *hdr = (WAVEHDR *)p1;
    int frames = (int)(hdr->dwBufferLength / (2 * sizeof(short)));
    if (frames > AS_WIN_BLOCK) frames = AS_WIN_BLOCK;
    if (frames > 0 && hdr->lpData) {
        const short *src = (const short *)hdr->lpData;
        float tmp[AS_WIN_BLOCK * 2];
        for (int i = 0; i < frames * 2; ++i) tmp[i] = (float)src[i] / 32768.0f;
        as_ring_write(&a->ring, tmp, frames);
    }
    waveInAddBuffer(h, hdr, sizeof(*hdr));
}

static void fill_output(AsAudio *a, short *dst, int frames) {
    int on = as_atomic_load_int(&a->tone_on);
    double freq = bits_double(as_atomic_load_u64(&a->freq_bits));
    double amp = bits_double(as_atomic_load_u64(&a->amp_bits));
    double step = on ? 2.0 * AS_PI * freq / a->fs : 0.0;
    for (int i = 0; i < frames; ++i) {
        double v = 0.0;
        if (on) {
            v = amp * sin(a->phase);
            a->phase += step;
            if (a->phase >= 2.0 * AS_PI) a->phase -= 2.0 * AS_PI;
        }
        short s = (short)(v * 32767.0);
        dst[2 * i] = s;
        dst[2 * i + 1] = s;
    }
}

static void CALLBACK out_callback(HWAVEOUT h, UINT msg, DWORD_PTR user,
                                  DWORD_PTR p1, DWORD_PTR p2) {
    (void)p2;
    AsAudio *a = (AsAudio *)user;
    if (!a || msg != WOM_DONE || !p1) return;
    WAVEHDR *hdr = (WAVEHDR *)p1;
    if (hdr->lpData) {
        int frames = (int)(hdr->dwBufferLength / (2 * sizeof(short)));
        if (frames > AS_WIN_BLOCK) frames = AS_WIN_BLOCK;
        fill_output(a, (short *)hdr->lpData, frames);
    }
    waveOutWrite(h, hdr, sizeof(*hdr));
}

AsAudio *as_audio_open(int in_index, int out_index, double fs, int block) {
    set_error("");
    if (!(fs >= 8000.0) || fs > 192000.0) {
        set_error("unsupported sample rate");
        return NULL;
    }
    AsAudio *a = (AsAudio *)calloc(1, sizeof(*a));
    if (!a) {
        set_error("out of memory");
        return NULL;
    }
    a->fs = fs;
    a->block = block > 0 ? block : AS_WIN_BLOCK;
#if defined(_MSC_VER)
    a->tone_on = 0;
    a->freq_bits = (LONG64)double_bits(1000.0);
    a->amp_bits = (LONG64)double_bits(0.1);
#else
    atomic_init(&a->tone_on, 0);
    atomic_init(&a->freq_bits, double_bits(1000.0));
    atomic_init(&a->amp_bits, double_bits(0.1));
#endif
    if (as_ring_init(&a->ring, (uint32_t)(fs * 4.0), 2) != 0) {
        set_error("ring allocation failed");
        free(a);
        return NULL;
    }

    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = (DWORD)fs;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    UINT in_id = in_index < 0 ? WAVE_MAPPER : (UINT)in_index;
    UINT out_id = out_index < 0 ? WAVE_MAPPER : (UINT)out_index;

    MMRESULT mr = MMSYSERR_NOERROR;
    if (in_index != -2) {
        mr = waveInOpen(&a->in, in_id, &fmt, (DWORD_PTR)in_callback,
                        (DWORD_PTR)a, CALLBACK_FUNCTION);
        if (mr != MMSYSERR_NOERROR) {
            a->in = NULL;
            set_error("waveInOpen failed");
        }
    }
    for (int i = 0; i < AS_WIN_BUFFERS && a->in; ++i) {
        memset(&a->in_hdr[i], 0, sizeof(a->in_hdr[i]));
        a->in_hdr[i].lpData = (LPSTR)a->in_buf[i];
        a->in_hdr[i].dwBufferLength = AS_WIN_BLOCK * 2 * (DWORD)sizeof(short);
        if (waveInPrepareHeader(a->in, &a->in_hdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            set_error("waveInPrepareHeader failed");
            break;
        }
        waveInAddBuffer(a->in, &a->in_hdr[i], sizeof(WAVEHDR));
    }

    if (out_index != -2) {
        mr = waveOutOpen(&a->out, out_id, &fmt, (DWORD_PTR)out_callback,
                         (DWORD_PTR)a, CALLBACK_FUNCTION);
        if (mr != MMSYSERR_NOERROR) {
            a->out = NULL;
            set_error("waveOutOpen failed");
        }
    }
    for (int i = 0; i < AS_WIN_BUFFERS && a->out; ++i) {
        memset(&a->out_hdr[i], 0, sizeof(a->out_hdr[i]));
        a->out_hdr[i].lpData = (LPSTR)a->out_buf[i];
        a->out_hdr[i].dwBufferLength = AS_WIN_BLOCK * 2 * (DWORD)sizeof(short);
        fill_output(a, a->out_buf[i], AS_WIN_BLOCK);
        if (waveOutPrepareHeader(a->out, &a->out_hdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            set_error("waveOutPrepareHeader failed");
            break;
        }
        waveOutWrite(a->out, &a->out_hdr[i], sizeof(WAVEHDR));
    }

    if (!a->in && !a->out) {
        as_audio_close(a);
        set_error("no audio input or output");
        return NULL;
    }
    if (a->in) waveInStart(a->in);
    a->running = 1;
    return a;
}

void as_audio_close(AsAudio *a) {
    if (!a) return;
    if (a->in) {
        waveInStop(a->in);
        waveInReset(a->in);
        for (int i = 0; i < AS_WIN_BUFFERS; ++i) {
            waveInUnprepareHeader(a->in, &a->in_hdr[i], sizeof(WAVEHDR));
        }
        waveInClose(a->in);
    }
    if (a->out) {
        waveOutReset(a->out);
        for (int i = 0; i < AS_WIN_BUFFERS; ++i) {
            waveOutUnprepareHeader(a->out, &a->out_hdr[i], sizeof(WAVEHDR));
        }
        waveOutClose(a->out);
    }
    as_ring_free(&a->ring);
    free(a);
}

int as_audio_read(AsAudio *a, float *dst, int frames) {
    if (!a || !dst || frames <= 0) return 0;
    return as_ring_read_latest(&a->ring, dst, frames);
}

int as_audio_set_tone(AsAudio *a, double freq, double amp, int on) {
    if (!a) return -1;
    if (!(freq >= 1.0) || freq > 20000.0) return -1;
    if (!(amp >= 0.0) || amp > 1.0) return -1;
    as_atomic_store_u64(&a->freq_bits, double_bits(freq));
    as_atomic_store_u64(&a->amp_bits, double_bits(amp));
    as_atomic_store_int(&a->tone_on, on ? 1 : 0);
    return 0;
}

double as_audio_rate(const AsAudio *a) {
    return a ? a->fs : 0.0;
}

int as_audio_running(const AsAudio *a) {
    return a ? a->running : 0;
}
