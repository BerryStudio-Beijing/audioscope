/* audio path smoke test: ./audiotest */
#include "../src/audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    ASDevice devs[48];
    int n = as_devices(devs, 48);
    printf("devices: %d\n", n);
    for (int i = 0; i < n; i++)
        printf("  [%u] %-42s in=%d out=%d %.0f Hz %s%s\n",
               (unsigned)devs[i].id, devs[i].name, devs[i].in_ch,
               devs[i].out_ch, devs[i].rate,
               (int)devs[i].id == as_default_device(0) ? "default-in " : "",
               (int)devs[i].id == as_default_device(1) ? "default-out" : "");

    ASEngine *e = (ASEngine *)calloc(1, sizeof(ASEngine));
    pthread_mutex_init(&e->lock, NULL);
    pthread_mutex_init(&e->cfg_lock, NULL);
    e->gen_cfg.wave = W_SINE;
    e->gen_cfg.freq = 440.0;
    e->gen_cfg.amp_db = -46.0;
    e->gen_cfg.enabled = 1;
    e->gen_cfg.ch_en[0] = 1;
    e->route = 0;

    int in_dev = as_default_device(0);
    int out_dev = as_default_device(1);
    int rc = engine_start(e, (AudioDeviceID)in_dev, (AudioDeviceID)out_dev,
                          48000.0, 512);
    printf("engine_start rc=%d fs=%.0f in=%.0f in_ch=%d\n", rc, e->fs,
           e->in_rate, e->in_ch);
    if (rc != 0) return 1;

    usleep(1200000);
    float *buf = (float *)malloc(48000 * 2 * sizeof(float));
    int64_t got = engine_read(e, buf, 24000);
    double peak = 0;
    for (int64_t i = 0; i < got * 2; i++) if (fabs(buf[i]) > peak) peak = fabs(buf[i]);
    printf("captured %lld frames  peak %.4f (%.1f dBFS)  in_cb=%d out_cb=%d xrun=%d\n",
           (long long)got, peak, as_db(peak), e->in_cb_count, e->out_cb_count,
           e->xruns);

    TimeStats st;
    if (got > 1000) {
        time_stats(buf + 2 * 1000, (int)(got - 1000), e->fs, 1.0, &st);
        printf("measured freq %.2f Hz  vpp %.4f  rms %.4f\n", st.freq, st.vpp,
               st.ac_rms);
    }
    engine_stop(e);
    free(buf);
    printf("AUDIO TEST DONE\n");
    return 0;
}
