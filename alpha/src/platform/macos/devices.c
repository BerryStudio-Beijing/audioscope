/* AudioScope by BerryStudio 0.3-alpha — macOS CoreAudio device enumeration. */
#include "core/api.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int channel_count(AudioDeviceID dev, int input) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyStreamConfiguration,
        input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(dev, &addr, 0, NULL, &size) != noErr ||
        size == 0) {
        return 0;
    }
    AudioBufferList *list = (AudioBufferList *)malloc(size);
    if (!list) return 0;
    int channels = 0;
    if (AudioObjectGetPropertyData(dev, &addr, 0, NULL, &size, list) == noErr) {
        for (UInt32 i = 0; i < list->mNumberBuffers; ++i) {
            channels += (int)list->mBuffers[i].mNumberChannels;
        }
    }
    free(list);
    return channels;
}

static int device_name(AudioDeviceID dev, char *out, size_t n) {
    AudioObjectPropertyAddress addr = {
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef name = NULL;
    UInt32 size = sizeof(name);
    if (AudioObjectGetPropertyData(dev, &addr, 0, NULL, &size, &name) != noErr ||
        !name) {
        snprintf(out, n, "Device %u", (unsigned)dev);
        return 0;
    }
    Boolean ok = CFStringGetCString(name, out, (CFIndex)n, kCFStringEncodingUTF8);
    CFRelease(name);
    if (!ok) snprintf(out, n, "Device %u", (unsigned)dev);
    return 0;
}

static double device_rate(AudioDeviceID dev) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    Float64 rate = 0.0;
    UInt32 size = sizeof(rate);
    if (AudioObjectGetPropertyData(dev, &addr, 0, NULL, &size, &rate) != noErr) {
        return 0.0;
    }
    return (double)rate;
}

int as_platform_list_devices(AsDeviceInfo *out, int max) {
    if (!out || max <= 0) return 0;
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, NULL,
                                       &size) != noErr ||
        size == 0) {
        return 0;
    }
    AudioDeviceID *ids = (AudioDeviceID *)malloc(size);
    if (!ids) return 0;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL,
                                   &size, ids) != noErr) {
        free(ids);
        return 0;
    }
    int count = (int)(size / sizeof(AudioDeviceID));
    int n = 0;
    for (int i = 0; i < count && n < max; ++i) {
        int in_ch = channel_count(ids[i], 1);
        int out_ch = channel_count(ids[i], 0);
        if (in_ch == 0 && out_ch == 0) continue;
        AsDeviceInfo *d = &out[n];
        memset(d, 0, sizeof(*d));
        snprintf(d->id, sizeof(d->id), "%u", (unsigned)ids[i]);
        device_name(ids[i], d->name, sizeof(d->name));
        d->is_input = in_ch > 0;
        d->is_output = out_ch > 0;
        d->channels = in_ch > out_ch ? in_ch : out_ch;
        d->in_channels = in_ch;
        d->out_channels = out_ch;
        d->sample_rate = device_rate(ids[i]);
        ++n;
    }
    free(ids);
    return n;
}

const char *as_platform_name(void) {
    return "macos-coreaudio";
}
