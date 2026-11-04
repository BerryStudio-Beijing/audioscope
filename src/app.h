/* AudioScope C — application state and logic. */
#ifndef AS_APP_H
#define AS_APP_H

#include "audio.h"
#include "calib.h"
#include "dsp.h"
#include "loopback.h"
#include "ui.h"

#define SCOPE_MAX  4096
#define SPEC_COLS  512
#define WF_COLS    256
#define WF_ROWS    480
#define BODE_MAX   400
#define MSG_LEN    200

enum { TAB_SCOPE = 0, TAB_FFT, TAB_BODE, TAB_LOOP, TAB_N };
enum { WIN_MAIN = 0, WIN_SOURCE };

typedef struct {
    float ch1[SCOPE_MAX], ch2[SCOPE_MAX], math[SCOPE_MAX], ref[SCOPE_MAX];
    int n, has_math, has_ref;
    double span, dt;
} ScopeTrace;

typedef struct {
    float db[3][SPEC_COLS];   /* ch1, ch2, peak hold */
    double marks_f[11];
    float marks_db[11];
    int mark_n, n;
} SpecTrace;

typedef struct {
    unsigned char *rgba;
    int w, h, row, count;
} Waterfall;

typedef struct {
    double f[BODE_MAX], g[BODE_MAX], ph[BODE_MAX];
    int n;
} BodeTrace;

typedef struct {
    /* ---- settings ---- */
    int in_dev, out_dev;         /* AudioDeviceID */
    double req_rate;
    double volts_at_fs, ch_gain[2], gen_volts;
    GenCfg gen;
    int route, muted;
    int trig_src, trig_edge;
    double trig_level, trig_hyst;
    double time_div, volt_div, v_off;
    int coupling, math, show_ch[2], average;
    int fft_size_idx, fft_win, peak_hold;
    double fft_avg;
    int wf_fft_idx, wf_cmap;
    double wf_span, wf_low, wf_high;
    double spec_low, spec_high;
    double bode_f1, bode_f2;
    int bode_points, bode_ref, bode_dut;
    double file_gain_db;
    char last_dir[512];

    /* ---- runtime ---- */
    ASEngine eng;
    double fs;
    int running, tab, frozen;
    double now, last_fft, last_scope;

    float *cap;            /* capture scratch, stereo frames */
    int cap_frames;
    float *mono1, *mono2, *fftbuf;
    double *dbuf, *win;
    FFT fft, wfft;
    double *spec_amp, *spec_freq, *logtmp, *wf_win;
    float *spec_lin, *spec_peak;

    ScopeTrace scope;
    SpecTrace spec;
    Waterfall wf;
    BodeTrace bode;
    TimeStats tstat[2];
    ToneMetrics tone[2];
    int tone_valid[2];

    int bode_active, bode_idx;
    double bode_next;
    char msg[MSG_LEN];
    double msg_time;

    ASDevice devs[48];
    int ndev;
    int rate_choices[8];
    int nrate, rate_idx;

    /* ---- 1.1 闭环测试 ---- */
    LbConfig lbcfg;
    Loopback lb;
    CalibData cal;
    int theme;
    char out_dir[512];
    int want_lb_start, want_lb_cal, want_lb_report;

    int recording;
    char record_name[256];
    int want_source;
    int want_quit;
    int want_save_wav;
    int want_save_csv;
    int want_save_bode;
    int want_open_file;
    int want_open_dir;
    int want_shot;

    UI ui;
    UI ui_src;
    double half_sample;
} App;

void app_init(App *a);
void app_start(App *a);
void app_stop(App *a);
void app_tick(App *a);
void app_draw(App *a, CGContextRef ctx, double w, double h, int window);
int  app_draw_ui(App *a, CGContextRef ctx, double w, double h, int window);
void app_auto_setup(App *a);
void app_reset_view(App *a);
void app_zoom_time(App *a, int dir);
void app_zoom_volt(App *a, int dir);
int  app_export_wav(App *a, const char *path);
int  app_export_csv(App *a, const char *path);
int  app_export_bode(App *a, const char *path);
void app_set_message(App *a, const char *fmt, ...);
void app_load_settings(App *a);
void app_save_settings(App *a);
void app_apply_devices(App *a, int in_id, int out_id);
void app_load_file(App *a, const char *path);
void app_ensure_fft_pub(App *a);

/* 1.1 新接口 */
void app_lb_start(App *a, int calibrate);
void app_lb_write_report(App *a);
void app_sync_gen(App *a);
int  app_theme_toggle(App *a);
void as_cmap(int palette, float x, unsigned char out[4]);

/* option label helpers (used by the views) */
const char *as_wave_name(int w);
const char *as_win_name(int w);
const char *as_cmap_name(int c);
const char *as_route_name(int r);
const char **as_tab_items(void);
const char **as_src_items(void);
const char **as_edge_items(void);
const char **as_coupling_items(void);
const char **as_math_items(void);
const char **as_win_items(void);
const char **as_cmap_items(void);
const char **as_route_items(void);
int as_rate(int idx);
int as_rate_count(void);
int as_fft_size(int idx);
int as_wf_size(int idx);
int as_tab_count(void);

#endif
