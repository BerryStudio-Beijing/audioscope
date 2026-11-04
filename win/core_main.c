/* AudioScope 1.1 — Windows 命令行核心（无 GUI，纯 C，可交叉编译）。
 *
 * 只使用 C 标准库：波形生成、测量、校准、报告整条链路都能跑；
 * 音频 IO 与界面属于平台层，Windows 版需要单独的 WASAPI / Direct2D 后端。
 */
#include <stdio.h>
#include <string.h>

#include "wavegen.h"

int run_selftest(void);
int run_unittest(void);
int run_simulate(const char *dir);

static void usage(void) {
    printf("AudioScope 1.1 core (Windows CLI)\n");
    printf("  --selftest          DSP self tests\n");
    printf("  --unittest          waveform / analysis / report unit tests\n");
    printf("  --simulate <dir>    run the measurement chain on a virtual DUT\n");
    printf("  --wavecheck         print waveform accuracy table\n");
    printf("  --help\n");
}

static int wavecheck(void) {
    const char *names[WG_COUNT] = {0};
    printf("%-11s %9s %9s %9s %9s\n", "wave", "amp(dB)", "freq(ppm)", "THD(%)",
           "spur(dBc)");
    for (int w = 0; w < WG_COUNT; w++) {
        WgConfig c;
        wg_default(&c, 48000.0);
        c.wave = w;
        c.f1 = c.f2 = 1000.0;
        c.amp = 0.5;
        WgCheck k;
        if (wg_check(&c, 32768, &k) != 0) continue;
        printf("%-11s %9.4f %9.2f %9.4f %9.1f\n", wg_name(w), k.amp_err_db,
               k.freq_err_ppm, k.thd_percent, k.spur_dbc);
    }
    (void)names;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 0;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage();
        return 0;
    }
    if (!strcmp(argv[1], "--selftest")) return run_selftest();
    if (!strcmp(argv[1], "--unittest")) return run_unittest();
    if (!strcmp(argv[1], "--wavecheck")) return wavecheck();
    if (!strcmp(argv[1], "--simulate"))
        return run_simulate(argc > 2 ? argv[2] : "sim");
    usage();
    return 2;
}
