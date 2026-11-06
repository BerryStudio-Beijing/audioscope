/* AudioScope by BerryStudio 0.2-alpha — macOS CoreAudio backend (2ch float32). */
#include "platform/audio_backend.h"
#include "core/api.h"
#include "core/ring.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AS_MAX_BLOCK 4096
#define AS_PI 3.14159265358979323846

struct AsAudio {
    AudioDeviceID in_dev;
    AudioDeviceID out_dev;
    AudioDeviceIOProcID in_proc;
    AudioDeviceIOProcID out_proc;
    int running;
    double fs;
    int block;
    AsRing ring;
    _Atomic int tone_on;
    _Atomic uint64_t freq_bits;
    _Atomic uint64_t amp_bits;
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

static double device_rate(AudioDeviceID dev) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    Float64 rate = 0.0;
    UInt32 size = sizeof(rate);
    if (AudioObjectGetPropertyData(dev, &addr, 0, NULL, &size, &rate) != noErr)
        return 0.0;
    return (double)rate;
}

static void set_device_rate(AudioDeviceID dev, double rate) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    Float64 r = rate;
    AudioObjectSetPropertyData(dev, &addr, 0, NULL, sizeof(r), &r);
}

static AudioDeviceID default_device(int input) {
    AudioObjectPropertyAddress addr = {
        input ? kAudioHardwarePropertyDefaultInputDevice
              : kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    AudioDeviceID dev = 0;
    UInt32 size = sizeof(dev);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL,
                                   &size, &dev) != noErr)
        return 0;
    return dev;
}

static int frames_from_buffer(const AudioBuffer *buf) {
    if (!buf || buf->mNumberChannels == 0) return 0;
    return (int)(buf->mDataByteSize / (sizeof(float) * buf->mNumberChannels));
}

static OSStatus input_proc(AudioDeviceID dev, const AudioTimeStamp *now,
                           const AudioBufferList *in_data,
                           const AudioTimeStamp *in_time,
                           AudioBufferList *out_data,
                           const AudioTimeStamp *out_time, void *client) {
    (void)dev; (void)now; (void)in_time; (void)out_data; (void)out_time;
    AsAudio *a = (AsAudio *)client;
    if (!a || !in_data || in_data->mNumberBuffers == 0) return noErr;

    int frames = frames_from_buffer(&in_data->mBuffers[0]);
    if (frames <= 0) return noErr;
    if (frames > AS_MAX_BLOCK) frames = AS_MAX_BLOCK;

    float tmp[AS_MAX_BLOCK * 2];
    if (in_data->mNumberBuffers == 1) {
        const float *src = (const float *)in_data->mBuffers[0].mData;
        UInt32 ch = in_data->mBuffers[0].mNumberChannels;
        if (!src) return noErr;
        for (int i = 0; i < frames; ++i) {
            tmp[2 * i] = src[(size_t)i * ch];
            tmp[2 * i + 1] = ch > 1 ? src[(size_t)i * ch + 1] : tmp[2 * i];
        }
    } else {
        const float *l = (const float *)in_data->mBuffers[0].mData;
        const float *r = (const float *)in_data->mBuffers[1].mData;
        for (int i = 0; i < frames; ++i) {
            tmp[2 * i] = l ? l[i] : 0.0f;
            tmp[2 * i + 1] = r ? r[i] : tmp[2 * i];
        }
    }
    as_ring_write(&a->ring, tmp, frames);
    return noErr;
}

static OSStatus output_proc(AudioDeviceID dev, const AudioTimeStamp *now,
                            const AudioBufferList *in_data,
                            const AudioTimeStamp *in_time,
                            AudioBufferList *out_data,
                            const AudioTimeStamp *out_time, void *client) {
    (void)dev; (void)now; (void)in_data; (void)in_time; (void)out_time;
    AsAudio *a = (AsAudio *)client;
    if (!a || !out_data || out_data->mNumberBuffers == 0) return noErr;

    int frames = frames_from_buffer(&out_data->mBuffers[0]);
    if (frames <= 0) return noErr;
    if (frames > AS_MAX_BLOCK) frames = AS_MAX_BLOCK;

    float tmp[AS_MAX_BLOCK * 2];
    int on = atomic_load_explicit(&a->tone_on, memory_order_relaxed);
    double freq = bits_double(atomic_load_explicit(&a->freq_bits, memory_order_relaxed));
    double amp = bits_double(atomic_load_explicit(&a->amp_bits, memory_order_relaxed));
    double step = on ? 2.0 * AS_PI * freq / a->fs : 0.0;
    for (int i = 0; i < frames; ++i) {
        float v = 0.0f;
        if (on) {
            v = (float)(amp * sin(a->phase));
            a->phase += step;
            if (a->phase >= 2.0 * AS_PI) a->phase -= 2.0 * AS_PI;
        }
        tmp[2 * i] = v;
        tmp[2 * i + 1] = v;
    }

    if (out_data->mNumberBuffers == 1) {
        float *dst = (float *)out_data->mBuffers[0].mData;
        UInt32 ch = out_data->mBuffers[0].mNumberChannels;
        if (!dst) return noErr;
        for (int i = 0; i < frames; ++i) {
            dst[(size_t)i * ch] = tmp[2 * i];
            if (ch > 1) dst[(size_t)i * ch + 1] = tmp[2 * i + 1];
        }
    } else {
        float *l = (float *)out_data->mBuffers[0].mData;
        float *r = (float *)out_data->mBuffers[1].mData;
        for (int i = 0; i < frames; ++i) {
            if (l) l[i] = tmp[2 * i];
            if (r) r[i] = tmp[2 * i + 1];
        }
    }
    return noErr;
}

AsAudio *as_audio_open(int in_index, int out_index, double fs, int block) {
    set_error("");
    if (!(fs >= 8000.0) || fs > 384000.0) {
        set_error("unsupported sample rate");
        return NULL;
    }
    AsDeviceInfo devs[64];
    int n = as_platform_list_devices(devs, 64);

    AudioDeviceID in_dev = 0, out_dev = 0;
    if (in_index != -2) {
        if (in_index >= 0 && in_index < n && devs[in_index].is_input) {
            in_dev = (AudioDeviceID)strtoul(devs[in_index].id, NULL, 10);
        } else {
            in_dev = default_device(1);
        }
    }
    if (out_index != -2) {
        if (out_index >= 0 && out_index < n && devs[out_index].is_output) {
            out_dev = (AudioDeviceID)strtoul(devs[out_index].id, NULL, 10);
        } else {
            out_dev = default_device(0);
        }
    }
    if (!in_dev && !out_dev) {
        set_error("no input or output device");
        return NULL;
    }

    AsAudio *a = (AsAudio *)calloc(1, sizeof(*a));
    if (!a) {
        set_error("out of memory");
        return NULL;
    }
    a->in_dev = in_dev;
    a->out_dev = out_dev;
    a->fs = fs;
    a->block = block > 0 ? block : 128;
    atomic_init(&a->tone_on, 0);
    atomic_init(&a->freq_bits, double_bits(1000.0));
    atomic_init(&a->amp_bits, double_bits(0.1));
    if (as_ring_init(&a->ring, (uint32_t)(fs * 4.0), 2) != 0) {
        set_error("ring allocation failed");
        free(a);
        return NULL;
    }

    if (out_dev) set_device_rate(out_dev, fs);
    if (in_dev) set_device_rate(in_dev, fs);
    double actual = out_dev ? device_rate(out_dev) : device_rate(in_dev);
    if (actual > 0.0) a->fs = actual;

    if (in_dev) {
        OSStatus st = AudioDeviceCreateIOProcID(in_dev, input_proc, a, &a->in_proc);
        if (st != noErr) {
            set_error("AudioDeviceCreateIOProcID(input) failed");
            as_audio_close(a);
            return NULL;
        }
    }
    if (out_dev) {
        OSStatus st = AudioDeviceCreateIOProcID(out_dev, output_proc, a, &a->out_proc);
        if (st != noErr) {
            set_error("AudioDeviceCreateIOProcID(output) failed");
            as_audio_close(a);
            return NULL;
        }
    }
    if (out_dev && AudioDeviceStart(out_dev, a->out_proc) != noErr) {
        set_error("AudioDeviceStart(output) failed");
        as_audio_close(a);
        return NULL;
    }
    if (in_dev && AudioDeviceStart(in_dev, a->in_proc) != noErr) {
        set_error("AudioDeviceStart(input) failed");
        as_audio_close(a);
        return NULL;
    }
    a->running = 1;
    return a;
}

void as_audio_close(AsAudio *a) {
    if (!a) return;
    if (a->out_proc && a->out_dev) {
        AudioDeviceStop(a->out_dev, a->out_proc);
        AudioDeviceDestroyIOProcID(a->out_dev, a->out_proc);
    }
    if (a->in_proc && a->in_dev) {
        AudioDeviceStop(a->in_dev, a->in_proc);
        AudioDeviceDestroyIOProcID(a->in_dev, a->in_proc);
    }
    a->running = 0;
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
    atomic_store_explicit(&a->freq_bits, double_bits(freq), memory_order_relaxed);
    atomic_store_explicit(&a->amp_bits, double_bits(amp), memory_order_relaxed);
    atomic_store_explicit(&a->tone_on, on ? 1 : 0, memory_order_relaxed);
    return 0;
}

double as_audio_rate(const AsAudio *a) {
    return a ? a->fs : 0.0;
}

int as_audio_running(const AsAudio *a) {
    return a ? a->running : 0;
}
