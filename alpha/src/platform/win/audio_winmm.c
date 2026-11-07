/* AudioScope by BerryStudio 0.7-alpha — Windows WinMM backend (2ch int16, 48 kHz).
 *
 * This is the alpha fallback. It is deliberately simple: 4 queued buffers,
 * int16 PCM, WAVE_MAPPER by default. WASAPI is the v1.2 target.
 */
#include "platform/audio_backend.h"
#include "core/api.h"
#include "core/generator.h"
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
    AsAtomicInt wave;
    AsAtomicU64 freq_bits;
    AsAtomicU64 freq_end_bits;
    AsAtomicU64 amp_bits;
    AsAtomicU64 period_bits;
    AsGenerator gen;
    float scratch[AS_WIN_BLOCK];
    int fade;      /* 启动淡入已输出样本数（仅音频线程访问） */
    int fade_len;  /* 200 ms */
    int prev_on;
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
    const int on = as_atomic_load_int(&a->tone_on);
    if (on && !a->prev_on) a->fade = 0;
    if (!on) a->fade = 0;
    a->prev_on = on;
    if (on) {
        AsGenConfig cfg;
        cfg.wave = as_atomic_load_int(&a->wave);
        cfg.freq_hz = bits_double(as_atomic_load_u64(&a->freq_bits));
        cfg.freq_end_hz = bits_double(as_atomic_load_u64(&a->freq_end_bits));
        cfg.amp = bits_double(as_atomic_load_u64(&a->amp_bits));
        cfg.period_s = bits_double(as_atomic_load_u64(&a->period_bits));
        as_gen_render(&a->gen, &cfg, a->scratch, frames);
    }
    for (int i = 0; i < frames; ++i) {
        double v = on ? (double)a->scratch[i] : 0.0;
        /* 启动 200 ms 淡入：输出设备刚打开时不硬切到满电平。 */
        if (on && a->fade < a->fade_len) {
            ++a->fade;
            v *= (double)a->fade / (double)a->fade_len;
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
    a->wave = AS_WAVE_SINE;
    a->freq_bits = (LONG64)double_bits(1000.0);
    a->freq_end_bits = (LONG64)double_bits(20000.0);
    a->amp_bits = (LONG64)double_bits(0.1);
    a->period_bits = (LONG64)double_bits(1.0);
#else
    atomic_init(&a->tone_on, 0);
    atomic_init(&a->wave, AS_WAVE_SINE);
    atomic_init(&a->freq_bits, double_bits(1000.0));
    atomic_init(&a->freq_end_bits, double_bits(20000.0));
    atomic_init(&a->amp_bits, double_bits(0.1));
    atomic_init(&a->period_bits, double_bits(1.0));
#endif
    as_generator_init(&a->gen, a->fs);
    a->fade = 0;
    a->fade_len = (int)(a->fs * 0.2);
    if (a->fade_len < 1) a->fade_len = 1;
    a->prev_on = 0;
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

    UINT in_id = WAVE_MAPPER;
    UINT out_id = WAVE_MAPPER;
    /* 列表下标 ≠ WinMM 设备 ID：输入和输出各有独立编号，必须按 id 解析。 */
    AsDeviceInfo devs[64];
    const int ndev = as_platform_list_devices(devs, 64);
    if (in_index >= 0 && in_index < ndev && devs[in_index].is_input &&
        strncmp(devs[in_index].id, "in", 2) == 0) {
        in_id = (UINT)strtoul(devs[in_index].id + 2, NULL, 10);
    }
    if (out_index >= 0 && out_index < ndev && devs[out_index].is_output &&
        strncmp(devs[out_index].id, "out", 3) == 0) {
        out_id = (UINT)strtoul(devs[out_index].id + 3, NULL, 10);
    }

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

int as_audio_set_generator(AsAudio *a, const AsGenConfig *cfg, int on) {
    if (!a || !cfg) return -1;
    if (!(cfg->freq_hz >= 1.0) || cfg->freq_hz > 20000.0) return -1;
    if (!(cfg->amp >= 0.0) || cfg->amp > 1.0) return -1;
    int wave = cfg->wave;
    if (wave < 0 || wave >= AS_WAVE_COUNT) return -1;
    as_atomic_store_u64(&a->freq_bits, double_bits(cfg->freq_hz));
    as_atomic_store_u64(&a->freq_end_bits, double_bits(cfg->freq_end_hz));
    as_atomic_store_u64(&a->amp_bits, double_bits(cfg->amp));
    as_atomic_store_u64(&a->period_bits, double_bits(cfg->period_s));
    as_atomic_store_int(&a->wave, wave);
    as_atomic_store_int(&a->tone_on, on ? 1 : 0);
    return 0;
}

int as_audio_set_tone(AsAudio *a, double freq, double amp, int on) {
    AsGenConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.wave = AS_WAVE_SINE;
    cfg.freq_hz = freq;
    cfg.freq_end_hz = freq;
    cfg.amp = amp;
    cfg.period_s = 1.0;
    return as_audio_set_generator(a, &cfg, on);
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
