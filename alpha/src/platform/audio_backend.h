/* AudioScope by BerryStudio 0.7-alpha — platform audio backend interface.
 *
 * `in_index` / `out_index` refer to entries returned by
 * as_platform_list_devices(); -1 means "system default".
 */
#ifndef AS_PLATFORM_AUDIO_BACKEND_H
#define AS_PLATFORM_AUDIO_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AsAudio AsAudio;

AsAudio   *as_audio_open(int in_index, int out_index, double fs, int block);
void       as_audio_close(AsAudio *audio);
int        as_audio_read(AsAudio *audio, float *dst, int frames);
int        as_audio_set_tone(AsAudio *audio, double freq, double amp, int on);
double     as_audio_rate(const AsAudio *audio);
int        as_audio_running(const AsAudio *audio);
const char *as_audio_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
