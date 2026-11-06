/* AudioScope by BerryStudio 0.5-alpha — Windows WinMM device enumeration.
 *
 * Alpha uses WinMM for a simple, dependency-free first Windows backend.
 * WASAPI/Direct2D are the v1.2 target; this file is intentionally small.
 */
#include "core/api.h"

#include <windows.h>
#include <mmsystem.h>

#include <stdio.h>
#include <string.h>

int as_platform_list_devices(AsDeviceInfo *out, int max) {
    if (!out || max <= 0) return 0;
    int n = 0;
    UINT inputs = waveInGetNumDevs();
    for (UINT i = 0; i < inputs && n < max; ++i) {
        WAVEINCAPS caps;
        if (waveInGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
        AsDeviceInfo *d = &out[n++];
        memset(d, 0, sizeof(*d));
        snprintf(d->id, sizeof(d->id), "in%u", i);
        snprintf(d->name, sizeof(d->name), "%s", caps.szPname);
        d->is_input = 1;
        d->is_output = 0;
        d->channels = caps.wChannels;
        d->in_channels = caps.wChannels;
        d->out_channels = 0;
        d->sample_rate = 48000.0;
    }
    UINT outputs = waveOutGetNumDevs();
    for (UINT i = 0; i < outputs && n < max; ++i) {
        WAVEOUTCAPS caps;
        if (waveOutGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
        AsDeviceInfo *d = &out[n++];
        memset(d, 0, sizeof(*d));
        snprintf(d->id, sizeof(d->id), "out%u", i);
        snprintf(d->name, sizeof(d->name), "%s", caps.szPname);
        d->is_input = 0;
        d->is_output = 1;
        d->channels = caps.wChannels;
        d->in_channels = 0;
        d->out_channels = caps.wChannels;
        d->sample_rate = 48000.0;
    }
    return n;
}

const char *as_platform_name(void) {
    return "windows-winmm";
}
