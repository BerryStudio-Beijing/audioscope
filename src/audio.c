#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kAudioObjectPropertyElementMain exists since macOS 12 (our minimum). */

/* ---- device enumeration ----------------------------------------------- */
static int channel_count(AudioDeviceID dev, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress a = {kAudioDevicePropertyStreamConfiguration,
                                    scope, kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(dev, &a, 0, NULL, &size) != noErr || size == 0)
        return 0;
    AudioBufferList *bl = (AudioBufferList *)malloc(size);
    if (!bl) return 0;
    int total = 0;
    if (AudioObjectGetPropertyData(dev, &a, 0, NULL, &size, bl) == noErr) {
        for (UInt32 i = 0; i < bl->mNumberBuffers; i++)
            total += (int)bl->mBuffers[i].mNumberChannels;
    }
    free(bl);
    return total;
}

double as_device_rate(AudioDeviceID dev) {
    AudioObjectPropertyAddress a = {kAudioDevicePropertyNominalSampleRate,
                                    kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    Float64 rate = 0;
    UInt32 size = sizeof(rate);
    if (AudioObjectGetPropertyData(dev, &a, 0, NULL, &size, &rate) == noErr)
        return (double)rate;
    return 0.0;
}

static void device_name(AudioDeviceID dev, char *buf, size_t n) {
    buf[0] = 0;
    AudioObjectPropertyAddress a = {kAudioObjectPropertyName,
                                    kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    CFStringRef name = NULL;
    UInt32 size = sizeof(name);
    if (AudioObjectGetPropertyData(dev, &a, 0, NULL, &size, &name) == noErr && name) {
        CFStringGetCString(name, buf, (CFIndex)n, kCFStringEncodingUTF8);
        CFRelease(name);
    }
    if (!buf[0]) snprintf(buf, n, "Device %u", (unsigned)dev);
}

int as_devices(ASDevice *out, int max) {
    AudioObjectPropertyAddress a = {kAudioHardwarePropertyDevices,
                                    kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL,
                                      &size) != noErr)
        return 0;
    int n = (int)(size / sizeof(AudioDeviceID));
    if (n <= 0) return 0;
    AudioDeviceID *ids = (AudioDeviceID *)malloc(size);
    if (!ids) return 0;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size,
                                   ids) != noErr) {
        free(ids);
        return 0;
    }
    int count = 0;
    for (int i = 0; i < n && count < max; i++) {
        int nin = channel_count(ids[i], kAudioDevicePropertyScopeInput);
        int nout = channel_count(ids[i], kAudioDevicePropertyScopeOutput);
        if (nin == 0 && nout == 0) continue;
        ASDevice *d = &out[count];
        memset(d, 0, sizeof(*d));
        d->id = ids[i];
        device_name(ids[i], d->name, sizeof(d->name));
        d->in_ch = nin;
        d->out_ch = nout;
        d->is_input = nin > 0;
        d->is_output = nout > 0;
        d->rate = as_device_rate(ids[i]);
        count++;
    }
    free(ids);
    return count;
}

int as_default_device(int output) {
    AudioObjectPropertyAddress a = {
        output ? kAudioHardwarePropertyDefaultOutputDevice
               : kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioDeviceID dev = 0;
    UInt32 size = sizeof(dev);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size,
                                   &dev) != noErr)
        return 0;
    return (int)dev;
}


/* ---- ring buffer ------------------------------------------------------ */

static uint32_t next_pow2u(uint32_t v) {
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

static void ring_write(ASEngine *e, const float *src, int frames, int src_ch) {
    if (frames <= 0) return;
    pthread_mutex_lock(&e->lock);
    for (int i = 0; i < frames; i++) {
        uint32_t w = e->ring_w & e->ring_mask;
        float l = src[i * src_ch];
        float r = src_ch > 1 ? src[i * src_ch + 1] : l;
        e->ring[2 * w] = l;
        e->ring[2 * w + 1] = r;
        e->ring_w++;
    }
    if (e->ring_filled + frames > (int64_t)e->ring_cap) {
        e->ring_filled = (int64_t)e->ring_cap;
        e->xruns++;
    } else {
        e->ring_filled += frames;
    }
    e->ring_total += (uint64_t)frames;
    pthread_mutex_unlock(&e->lock);
}

int64_t engine_read(ASEngine *e, float *dst, int64_t frames) {
    if (!e->ring) return 0;
    pthread_mutex_lock(&e->lock);
    int64_t avail = e->ring_filled;
    if (frames > avail) frames = avail;
    if (frames > (int64_t)e->ring_cap) frames = (int64_t)e->ring_cap;
    if (frames <= 0) {
        pthread_mutex_unlock(&e->lock);
        return 0;
    }
    uint32_t start = (e->ring_w - (uint32_t)frames) & e->ring_mask;
    for (int64_t i = 0; i < frames; i++) {
        uint32_t r = (start + (uint32_t)i) & e->ring_mask;
        dst[2 * i] = e->ring[2 * r];
        dst[2 * i + 1] = e->ring[2 * r + 1];
    }
    pthread_mutex_unlock(&e->lock);
    return frames;
}

void engine_reset_stats(ASEngine *e) {
    e->xruns = 0;
    e->in_peak[0] = e->in_peak[1] = 0;
    e->out_peak[0] = e->out_peak[1] = 0;
    e->in_rms[0] = e->in_rms[1] = 0;
}

/* ---- output rendering ------------------------------------------------- */
static void render_output(ASEngine *e, int frames) {
    if (frames <= 0) return;
    if (frames > AS_MAX_BLOCK) frames = AS_MAX_BLOCK;

    GenCfg cfg;
    pthread_mutex_lock(&e->cfg_lock);
    cfg = e->gen_cfg;
    pthread_mutex_unlock(&e->cfg_lock);

    const int route = e->route;
    const int muted = e->muted;
    const float fgain = e->file_gain;
    int playing = e->playing;

    if (e->gen.samples == 0) e->gen.start_frame = e->out_frames;
    gen_render(&e->gen, &cfg, e->scratch, frames);
    e->out_frames += (uint64_t)frames;
    const float *use = e->scratch;
    if (route != 0) {
        ASFile *f = &e->file;
        for (int i = 0; i < frames; i++) {
            float l = 0.0f, r = 0.0f;
            if (playing && f->data && f->frames > 0) {
                if (e->file_pos < f->frames) {
                    l = f->data[2 * e->file_pos] * fgain;
                    r = f->data[2 * e->file_pos + 1] * fgain;
                    e->file_pos++;
                } else if (e->loop) {
                    e->file_pos = 0;
                } else {
                    playing = 0;
                    e->playing = 0;
                    e->finished = 1;
                }
            }
            if (route == 2) {
                e->mixbuf[2 * i] = e->scratch[2 * i] + l;
                e->mixbuf[2 * i + 1] = e->scratch[2 * i + 1] + r;
            } else {
                e->mixbuf[2 * i] = l;
                e->mixbuf[2 * i + 1] = r;
            }
        }
        use = e->mixbuf;
    }

    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < frames; i++) {
        float l = muted ? 0.0f : use[2 * i];
        float r = muted ? 0.0f : use[2 * i + 1];
        if (l > 1.0f) l = 1.0f;
        if (l < -1.0f) l = -1.0f;
        if (r > 1.0f) r = 1.0f;
        if (r < -1.0f) r = -1.0f;
        float al = fabsf(l), ar = fabsf(r);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
        e->out_int[2 * i] = l;
        e->out_int[2 * i + 1] = r;
    }
    e->out_peak[0] = e->out_peak[0] * 0.7f + pl * 0.3f;
    e->out_peak[1] = e->out_peak[1] * 0.7f + pr * 0.3f;
}


/* map our interleaved stereo block into whatever the device wants */
static void write_device_buffer(ASEngine *e, AudioBufferList *abl, int frames) {
    for (UInt32 b = 0; b < abl->mNumberBuffers; b++) {
        AudioBuffer *buf = &abl->mBuffers[b];
        int nch = (int)buf->mNumberChannels;
        float *dst = (float *)buf->mData;
        if (!dst) continue;
        if (abl->mNumberBuffers == 1) {
            if (nch >= 2) {
                memcpy(dst, e->out_int, (size_t)frames * 2 * sizeof(float));
            } else {
                for (int i = 0; i < frames; i++) dst[i] = e->out_int[2 * i];
            }
        } else {
            int src = (b == 0) ? 0 : 1;
            for (int i = 0; i < frames; i++) dst[i] = e->out_int[2 * i + src];
        }
    }
}

static OSStatus out_ioproc(AudioObjectID dev, const AudioTimeStamp *now,
                           const AudioBufferList *inData,
                           const AudioTimeStamp *inTime, AudioBufferList *outData,
                           const AudioTimeStamp *outTime, void *client) {
    (void)dev; (void)now; (void)inData; (void)inTime; (void)outTime;
    ASEngine *e = (ASEngine *)client;
    if (!outData || outData->mNumberBuffers == 0) return noErr;
    UInt32 nch = outData->mBuffers[0].mNumberChannels;
    int frames = (int)(outData->mBuffers[0].mDataByteSize / (4 * (nch ? nch : 1)));
    render_output(e, frames);
    write_device_buffer(e, outData, frames);
    e->out_cb_count++;
    return noErr;
}

/* ---- input capture ---------------------------------------------------- */
static void push_input(ASEngine *e, const float *in, int n_in) {
    if (n_in <= 0) return;
    if (fabs(e->in_rate - e->fs) < 1e-6) {
        ring_write(e, in, n_in, 2);
        return;
    }
    const double step = e->in_rate / e->fs;   /* input frames per output frame */
    double pos = e->rs_phase;
    int outn = 0;
    while (outn < AS_MAX_BLOCK) {
        int i0 = (int)pos;                    /* virtual index: 0 = prev sample */
        if (i0 + 1 > n_in) break;
        double f = pos - (double)i0;
        float l0 = (i0 == 0) ? e->rs_prev[0] : in[2 * (i0 - 1)];
        float r0 = (i0 == 0) ? e->rs_prev[1] : in[2 * (i0 - 1) + 1];
        float l1 = in[2 * i0], r1 = in[2 * i0 + 1];
        e->in_tmp[2 * outn] = (float)(l0 * (1.0 - f) + l1 * f);
        e->in_tmp[2 * outn + 1] = (float)(r0 * (1.0 - f) + r1 * f);
        outn++;
        pos += step;
    }
    e->rs_phase = pos - (double)(n_in > 1 ? n_in - 1 : 1);
    if (e->rs_phase < 0) e->rs_phase = 0;
    e->rs_prev[0] = in[2 * (n_in - 1)];
    e->rs_prev[1] = in[2 * (n_in - 1) + 1];
    if (outn > 0) ring_write(e, e->in_tmp, outn, 2);
}

static OSStatus in_ioproc(AudioObjectID dev, const AudioTimeStamp *now,
                          const AudioBufferList *inData,
                          const AudioTimeStamp *inTime, AudioBufferList *outData,
                          const AudioTimeStamp *outTime, void *client) {
    (void)dev; (void)now; (void)inTime; (void)outData; (void)outTime;
    ASEngine *e = (ASEngine *)client;
    if (!inData || inData->mNumberBuffers == 0) return noErr;

    int nch0 = (int)inData->mBuffers[0].mNumberChannels;
    int frames = (int)(inData->mBuffers[0].mDataByteSize / (4 * (nch0 ? nch0 : 1)));
    if (frames <= 0) return noErr;
    if (frames > AS_MAX_BLOCK) frames = AS_MAX_BLOCK;

    if (inData->mNumberBuffers == 1 && nch0 >= 2) {
        const float *src = (const float *)inData->mBuffers[0].mData;
        for (int i = 0; i < frames; i++) {
            e->in_tmp[2 * i] = src[2 * i];
            e->in_tmp[2 * i + 1] = src[2 * i + 1];
        }
    } else if (inData->mNumberBuffers == 1) {
        const float *src = (const float *)inData->mBuffers[0].mData;
        for (int i = 0; i < frames; i++) {
            e->in_tmp[2 * i] = src[i];
            e->in_tmp[2 * i + 1] = src[i];
        }
    } else {
        const float *l = (const float *)inData->mBuffers[0].mData;
        const float *r = (const float *)inData->mBuffers[1].mData;
        for (int i = 0; i < frames; i++) {
            e->in_tmp[2 * i] = l ? l[i] : 0.0f;
            e->in_tmp[2 * i + 1] = r ? r[i] : e->in_tmp[2 * i];
        }
    }

    float pl = 0, pr = 0, sl = 0, sr = 0;
    for (int i = 0; i < frames; i++) {
        float l = e->in_tmp[2 * i], r = e->in_tmp[2 * i + 1];
        float al = fabsf(l), ar = fabsf(r);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
        sl += l * l;
        sr += r * r;
    }
    e->in_peak[0] = e->in_peak[0] * 0.6f + pl * 0.4f;
    e->in_peak[1] = e->in_peak[1] * 0.6f + pr * 0.4f;
    e->in_rms[0] = e->in_rms[0] * 0.8f + sqrtf(sl / (float)frames) * 0.2f;
    e->in_rms[1] = e->in_rms[1] * 0.8f + sqrtf(sr / (float)frames) * 0.2f;
    e->in_cb_count++;

    push_input(e, e->in_tmp, frames);
    return noErr;
}


/* ---- engine start / stop ---------------------------------------------- */
static void set_device_rate(AudioDeviceID dev, double rate) {
    AudioObjectPropertyAddress a = {kAudioDevicePropertyNominalSampleRate,
                                    kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    Float64 r = rate;
    AudioObjectSetPropertyData(dev, &a, 0, NULL, sizeof(r), &r);
}

int engine_start(ASEngine *e, AudioDeviceID in_dev, AudioDeviceID out_dev,
                 double rate, int block) {
    engine_stop(e);
    e->fs = rate > 0 ? rate : 48000.0;
    e->block = block > 0 ? block : 512;
    e->in_dev = in_dev;
    e->out_dev = out_dev;
    e->ring_cap = next_pow2u((uint32_t)(e->fs * 8.0));
    e->ring_mask = e->ring_cap - 1;
    free(e->ring);
    e->ring = (float *)calloc((size_t)e->ring_cap * 2, sizeof(float));
    if (!e->ring) return -1;
    e->ring_w = 0;
    e->ring_filled = 0;
    e->file_pos = 0;
    e->in_cb_count = 0;
    e->out_cb_count = 0;
    e->finished = 0;
    e->rs_phase = 0.0;
    e->rs_prev[0] = e->rs_prev[1] = 0.0f;
    memset((void *)e->in_peak, 0, sizeof(e->in_peak));
    memset((void *)e->out_peak, 0, sizeof(e->out_peak));
    memset((void *)e->in_rms, 0, sizeof(e->in_rms));

    if (out_dev) set_device_rate(out_dev, e->fs);
    if (in_dev) set_device_rate(in_dev, e->fs);
    double orate = out_dev ? as_device_rate(out_dev) : e->fs;
    if (orate > 0) e->fs = orate;
    gen_init(&e->gen, e->fs);
    e->in_rate = in_dev ? as_device_rate(in_dev) : e->fs;
    if (e->in_rate <= 0) e->in_rate = e->fs;
    e->in_ch = in_dev ? channel_count(in_dev, kAudioDevicePropertyScopeInput) : 0;
    if (e->in_ch <= 0) e->in_ch = 2;

    if (in_dev && AudioDeviceCreateIOProcID(in_dev, in_ioproc, e, &e->in_proc)
                      != noErr) {
        e->in_proc = NULL;
        engine_stop(e);
        return -3;
    }
    if (out_dev && AudioDeviceCreateIOProcID(out_dev, out_ioproc, e, &e->out_proc)
                       != noErr) {
        e->out_proc = NULL;
        engine_stop(e);
        return -2;
    }
    if (out_dev && AudioDeviceStart(out_dev, e->out_proc) != noErr) {
        engine_stop(e);
        return -2;
    }
    if (in_dev && AudioDeviceStart(in_dev, e->in_proc) != noErr) {
        engine_stop(e);
        return -3;
    }
    e->in_latency_ms = 1000.0 * (double)e->block / e->fs;
    e->out_latency_ms = 1000.0 * (double)e->block / e->fs;
    e->running = 1;
    return 0;
}

void engine_stop(ASEngine *e) {
    if (e->out_proc && e->out_dev) {
        AudioDeviceStop(e->out_dev, e->out_proc);
        AudioDeviceDestroyIOProcID(e->out_dev, e->out_proc);
        e->out_proc = NULL;
    }
    if (e->in_proc && e->in_dev) {
        AudioDeviceStop(e->in_dev, e->in_proc);
        AudioDeviceDestroyIOProcID(e->in_dev, e->in_proc);
        e->in_proc = NULL;
    }
    e->running = 0;
    e->playing = 0;
}


/* ---- audio file loading (ExtAudioFile decodes everything) ------------- */
void file_free(ASFile *f) {
    free(f->data);
    memset(f, 0, sizeof(*f));
}

int file_load(ASFile *f, const char *path, double target_rate) {
    file_free(f);
    CFStringRef ps = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    if (!ps) return -1;
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, ps, kCFURLPOSIXPathStyle,
                                                 false);
    CFRelease(ps);
    if (!url) return -1;

    ExtAudioFileRef af = NULL;
    if (ExtAudioFileOpenURL(url, &af) != noErr) {
        CFRelease(url);
        return -1;
    }
    CFRelease(url);

    AudioStreamBasicDescription ff;
    UInt32 sz = sizeof(ff);
    if (ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileDataFormat, &sz, &ff)
        != noErr) {
        ExtAudioFileDispose(af);
        return -1;
    }
    int ch = ff.mChannelsPerFrame > 1 ? 2 : 1;
    double src_rate = ff.mSampleRate > 0 ? ff.mSampleRate : target_rate;

    AudioStreamBasicDescription client;
    memset(&client, 0, sizeof(client));
    client.mSampleRate = target_rate;
    client.mFormatID = kAudioFormatLinearPCM;
    client.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    client.mBytesPerPacket = (UInt32)(4 * ch);
    client.mFramesPerPacket = 1;
    client.mBytesPerFrame = (UInt32)(4 * ch);
    client.mChannelsPerFrame = (UInt32)ch;
    client.mBitsPerChannel = 32;
    if (ExtAudioFileSetProperty(af, kExtAudioFileProperty_ClientDataFormat,
                                sizeof(client), &client) != noErr) {
        ExtAudioFileDispose(af);
        return -2;
    }

    SInt64 file_frames = 0;
    sz = sizeof(file_frames);
    ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames, &sz,
                            &file_frames);
    int64_t est = (int64_t)((double)file_frames * target_rate / src_rate) + 8192;
    if (est < 8192) est = 8192;
    float *buf = (float *)malloc((size_t)est * (size_t)ch * sizeof(float));
    if (!buf) {
        ExtAudioFileDispose(af);
        return -3;
    }

    int64_t got_total = 0;
    while (got_total < est) {
        UInt32 want = 8192;
        if (got_total + (int64_t)want > est) want = (UInt32)(est - got_total);
        if (want == 0) break;
        AudioBufferList abl;
        abl.mNumberBuffers = 1;
        abl.mBuffers[0].mNumberChannels = (UInt32)ch;
        abl.mBuffers[0].mDataByteSize = want * (UInt32)ch * sizeof(float);
        abl.mBuffers[0].mData = buf + got_total * ch;
        OSStatus st = ExtAudioFileRead(af, &want, &abl);
        if (st != noErr || want == 0) break;
        got_total += want;
    }
    ExtAudioFileDispose(af);

    if (got_total <= 0) {
        free(buf);
        return -4;
    }
    if (ch == 1) {                       /* expand mono in place (backwards) */
        for (int64_t i = got_total - 1; i >= 0; i--) {
            buf[2 * i] = buf[i];
            buf[2 * i + 1] = buf[i];
        }
    }
    f->data = buf;
    f->frames = got_total;
    f->ch = 2;
    f->duration = (double)got_total / target_rate;
    const char *base = strrchr(path, '/');
    snprintf(f->name, sizeof(f->name), "%s", base ? base + 1 : path);
    return 0;
}

