/* AudioScope C — CoreAudio engine (output + input units, ring buffer, file). */
#ifndef AS_AUDIO_H
#define AS_AUDIO_H

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioOutputUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <pthread.h>
#include <stdint.h>

#include "dsp.h"

#define AS_MAX_BLOCK 4096

typedef struct {
    AudioDeviceID id;
    char name[160];
    int is_input, is_output;
    int in_ch, out_ch;
    double rate;
} ASDevice;

typedef struct {
    float *data;      /* interleaved stereo at engine rate */
    int64_t frames;
    int ch;
    double duration;
    char name[256];
} ASFile;

typedef struct {
    AudioDeviceID in_dev, out_dev;
    AudioDeviceIOProcID out_proc, in_proc;
    int running;
    double fs;        /* system rate (from the output device) */
    double in_rate;   /* input device rate (resampled if it differs) */
    int block, in_ch;

    /* capture ring (interleaved stereo) */
    float *ring;
    uint32_t ring_cap, ring_mask, ring_w;
    int64_t ring_filled;
    pthread_mutex_t lock;

    /* meter / status */
    volatile float in_peak[2], in_rms[2], out_peak[2];
    volatile int xruns;
    volatile double in_latency_ms, out_latency_ms;
    volatile int in_cb_count;
    volatile int out_cb_count;
    volatile uint64_t out_frames;   /* 输出帧计数（含静音），用于延迟测量 */
    volatile uint64_t ring_total;   /* 写入 ring 的总帧数（绝对输入索引） */

    /* generator (owned by audio thread) */
    Gen gen;
    GenCfg gen_cfg;
    pthread_mutex_t cfg_lock;

    /* file player */
    ASFile file;
    int64_t file_pos;
    volatile int playing, loop, finished;
    float file_gain;

    volatile int route;   /* 0 gen, 1 file, 2 mix */
    volatile int muted;

    float out_int[AS_MAX_BLOCK * 2];
    float scratch[AS_MAX_BLOCK * 2];
    float mixbuf[AS_MAX_BLOCK * 2];
    float in_tmp[(AS_MAX_BLOCK + 2) * 2];
    float rs_prev[2];
    double rs_phase;
} ASEngine;

int  as_devices(ASDevice *out, int max);
int  as_default_device(int output);
double as_device_rate(AudioDeviceID dev);

int  engine_start(ASEngine *e, AudioDeviceID in_dev, AudioDeviceID out_dev,
                  double rate, int block);
void engine_stop(ASEngine *e);
int64_t engine_read(ASEngine *e, float *dst, int64_t frames); /* stereo frames */
void engine_reset_stats(ASEngine *e);

int  file_load(ASFile *f, const char *path, double target_rate);
void file_free(ASFile *f);

#endif
