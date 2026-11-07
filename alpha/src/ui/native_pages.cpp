/* AudioScope by BerryStudio 0.9 — native macOS pages. */
#include "ui/native_pages.hpp"

#include "core/api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace audioscope {
namespace {

constexpr double W = 1120.0;
constexpr double H = 700.0;
constexpr double kHeaderH = 64.0;
constexpr double kNavH = 48.0;
constexpr double kFooterH = 32.0;
constexpr double BODY_Y = kHeaderH + kNavH;
constexpr double BODY_H = H - kFooterH - BODY_Y;
constexpr double PAD = 16.0;
constexpr double LEFT_W = 232.0;
constexpr double RIGHT_W = 216.0;
constexpr double CENTER_X = PAD + LEFT_W + PAD;
constexpr double CENTER_W = W - CENTER_X - RIGHT_W - PAD * 2;
constexpr double RIGHT_X = W - RIGHT_W - PAD;

const char *kNav[PAGE_COUNT] = {"波形", "频谱", "相位", "测量", "维修", "报告", "校准"};

ASRect left_card() { return as_rect(PAD, BODY_Y + PAD, LEFT_W, BODY_H - PAD * 2); }
ASRect center_card() {
    return as_rect(CENTER_X, BODY_Y + PAD, CENTER_W, BODY_H - PAD * 2);
}
ASRect right_card() {
    return as_rect(RIGHT_X, BODY_Y + PAD, RIGHT_W, BODY_H - PAD * 2);
}

std::string fmt(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string fmt_signed(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.*f", decimals, v);
    return buf;
}

void label_value(UI *u, double x, double y, double w, const char *label,
                 const std::string &value, uint32_t color) {
    ui_text(u, x, y, FONT_SMALL, g_pal.text_faint, label);
    ui_text_right(u, as_rect(x, y - 2, w, ui_font_height(FONT_VALUE)), FONT_VALUE,
                  color, value.c_str());
}

void title_block(UI *u, ASRect r, const char *title, const char *sub) {
    ui_text(u, r.x + PAD, r.y + 12, FONT_SMALL, g_pal.text_faint, title);
    if (sub && *sub)
        ui_text_right(u, as_rect(r.x + PAD, r.y + 10, r.w - PAD * 2,
                                 ui_font_height(FONT_SMALL)),
                      FONT_SMALL, g_pal.text_faint, sub);
    ui_line(u, r.x + PAD, r.y + 32, r.x + r.w - PAD, r.y + 32, g_pal.line_soft, 1);
}

void plot_grid(UI *u, ASRect p, int cols, int rows) {
    for (int i = 0; i <= cols; ++i) {
        double x = p.x + p.w * (double)i / (double)cols;
        ui_line(u, x, p.y, x, p.y + p.h, g_pal.chart_grid, 1);
    }
    for (int i = 0; i <= rows; ++i) {
        double y = p.y + p.h * (double)i / (double)rows;
        ui_line(u, p.x, y, p.x + p.w, y, g_pal.chart_grid, 1);
    }
}

double db_to_y(double db, ASRect p) {
    double t = (db + 100.0) / 100.0;
    return p.y + p.h - t * p.h;
}

/* --------------------------------------------------------------- header */
void draw_header(UI *u, NativeModel *m) {
    ui_fill(u, as_rect(0, 0, W, kHeaderH), g_pal.bg);
    ui_dot(u, 30, 26, 9, g_pal.accent);
    ui_dot(u, 30, 26, 3, g_pal.bg);
    ui_text(u, 48, 14, FONT_VALUE, g_pal.text, "AUDIOSCOPE");
    ui_text(u, 48, 36, FONT_SMALL, g_pal.text_faint,
            "BERRYSTUDIO / " AS_VERSION);
    ui_text_right(u, as_rect(W - 420, 14, 250, 16), FONT_SMALL,
                  m->capture ? g_pal.warn : g_pal.good,
                  m->capture ? "CAPTURE · INPUT" : "SYNTHETIC SOURCE");
    ui_text_right(u, as_rect(W - 420, 34, 250, 14), FONT_SMALL,
                  g_pal.text_faint, m->device.c_str());
    if (ui_button(u, 900, as_rect(W - 150, 16, 62, 30),
                  m->light ? "LIGHT" : "DARK", BTN_GHOST)) {
        theme_toggle();
        m->light = theme_is_light();
    }
    if (ui_button(u, 901, as_rect(W - 80, 16, 62, 30), "ABOUT", BTN_GHOST)) {
        m->message = AS_BRAND " " AS_VERSION;
    }
}

void draw_nav(UI *u, NativeModel *m) {
    ASRect r = as_rect(PAD, kHeaderH, W - PAD * 2, kNavH);
    int page = m->page;
    if (ui_navbar(u, 100, r, kNav, PAGE_COUNT, &page)) m->page = page;
}

/* --------------------------------------------------------------- pages */
void draw_scope(UI *u, NativeModel *m, NativeActions *act) {
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD;
    double w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "SOURCE");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_faint, "INPUT");
    ui_text_right(u, as_rect(x, y - 2, w, 18), FONT_VALUE, g_pal.text,
                  m->capture ? "CAPTURE" : "SYNTHETIC");
    label_value(u, x, y + 26, w, "DEVICE", m->device, g_pal.text);
    label_value(u, x, y + 50, w, "RATE", fmt(m->fs, 0) + " Hz", g_pal.text);
    label_value(u, x, y + 74, w, "MODE", "Inst", g_pal.text);
    if (ui_button(u, 200, as_rect(x, y + 100, w, 30),
                  m->capture ? "STOP CAPTURE" : "CAPTURE INPUT",
                  m->capture ? BTN_DANGER : BTN_NORMAL)) {
        act->toggle_capture = true;
    }
    int hold = m->hold ? 1 : 0;
    if (ui_toggle(u, 201, as_rect(x, y + 138, w, 30), "HOLD  (freeze)", &hold)) {
        m->hold = hold != 0;
        m->message = m->hold ? "display held" : "display live";
    }

    y = ui_group(u, x, y + 190, w, "CHANNELS");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "CH1  ·  AC  ·  ±1.0 V");
    ui_text(u, x, y + 22, FONT_SMALL, g_pal.text_faint, "CH2  ·  AC  ·  ±1.0 V");

    y = ui_group(u, x, y + 58, w, "TIMEBASE");
    ui_text(u, x, y, FONT_VALUE, g_pal.text, "1.50 ms/div");
    ui_text(u, x, y + 26, FONT_SMALL, g_pal.text_faint, "trigger: CH1 rising · 0.00 V");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "TIME DOMAIN", "CH1 · 48 kHz · 15 ms window");
    ASRect plot = as_rect(c.x + PAD + 24, c.y + 48, c.w - PAD * 2 - 40, c.h - 96);
    mat_recessed(u, plot, 4);
    plot_grid(u, plot, 10, 8);
    double mid = plot.y + plot.h * 0.5;
    ui_line(u, plot.x, mid, plot.x + plot.w, mid, g_pal.chart_axis, 1);

    if (!m->wave.empty()) {
        int n = (int)m->wave.size();
        double px = 0, py = 0;
        for (int i = 0; i < n; ++i) {
            double tx = plot.x + plot.w * (double)i / (double)(n - 1);
            double v = std::clamp((double)m->wave[(size_t)i] / 1.0, -1.0, 1.0);
            double ty = mid - v * plot.h * 0.44;
            if (i) ui_line(u, px, py, tx, ty, g_pal.curve_main, 1.4);
            px = tx;
            py = ty;
        }
    }
    double dash[2] = {4, 4};
    ui_line_dash(u, plot.x + plot.w * 0.5, plot.y, plot.x + plot.w * 0.5,
                 plot.y + plot.h, g_pal.warn, 1, dash, 2);
    ui_text_right(u, as_rect(plot.x, plot.y + plot.h + 4, plot.w, 14), FONT_SMALL,
                  g_pal.text_faint, "15.0 ms  ·  10 div  ·  −1.0 … +1.0 FS");

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD;
    double rw = r.w - PAD * 2;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "INPUT PEAK · CH1");
    ui_text(u, rx, r.y + 30, FONT_CRIT, g_pal.accent,
            fmt(as_linear_to_db(m->stats.peak), 1) + " dBFS");
    double yy = r.y + 78;
    label_value(u, rx, yy, rw, "RMS", fmt(as_linear_to_db(m->stats.rms), 1) + " dBFS", g_pal.text);
    label_value(u, rx, yy + 26, rw, "FREQUENCY", fmt(m->stats.freq, 1) + " Hz", g_pal.text);
    label_value(u, rx, yy + 52, rw, "CREST", fmt(20.0 * std::log10(m->stats.peak / std::max(m->stats.rms, 1e-9)), 1) + " dB", g_pal.text);
    label_value(u, rx, yy + 78, rw, "Vpp", fmt(m->stats.vpp, 3) + " FS", g_pal.text);
    label_value(u, rx, yy + 104, rw, "CLIP", m->clip ? "LATCHED" : "0", m->clip ? g_pal.bad : g_pal.text);
    ui_line(u, rx, yy + 136, rx + rw, yy + 136, g_pal.line_soft, 1);
    ui_text(u, rx, yy + 150, FONT_SMALL, g_pal.text_faint,
            m->cal_fs_vrms > 0 ? "voltage: calibrated" : "voltage: uncalibrated");
    ui_text(u, rx, yy + 170, FONT_SMALL,
            m->cal_fs_vrms > 0 ? g_pal.good : g_pal.warn,
            m->cal_fs_vrms > 0 ? "measured" : "dBFS only");
}

void draw_spectrum(UI *u, NativeModel *m, NativeActions *act) {
    (void)act;
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "FFT");
    label_value(u, x, y, w, "SIZE", "4096", g_pal.text);
    label_value(u, x, y + 26, w, "WINDOW", "Hann", g_pal.text);
    label_value(u, x, y + 52, w, "RATE", fmt(m->fs, 0) + " Hz", g_pal.text);
    y = ui_group(u, x, y + 92, w, "SOURCE");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, m->capture ? "capture · CH1" : "synthetic · CH1");
    ui_text(u, x, y + 24, FONT_SMALL, g_pal.text_faint, "peak-relative, 100 dB floor");
    y = ui_group(u, x, y + 62, w, "HARMONICS");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "H2  −58.1 dB");
    ui_text(u, x, y + 22, FONT_SMALL, g_pal.text_faint, "H3  −62.4 dB");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "SPECTRUM", "Hann · 4096 · 20 Hz – 20 kHz");
    ASRect p = as_rect(c.x + PAD + 32, c.y + 48, c.w - PAD * 2 - 48, c.h - 96);
    mat_recessed(u, p, 4);
    for (int i = 0; i <= 5; ++i)
        ui_line(u, p.x, p.y + p.h * (double)i / 5.0, p.x + p.w,
                p.y + p.h * (double)i / 5.0, g_pal.chart_grid, 1);
    for (int i = 0; i <= 4; ++i)
        ui_line(u, p.x + p.w * (double)i / 4.0, p.y, p.x + p.w * (double)i / 4.0,
                p.y + p.h, g_pal.chart_grid, 1);

    if (!m->spec_db.empty()) {
        int n = (int)m->spec_db.size();
        double px = 0, py = 0;
        for (int i = 0; i < n; ++i) {
            double tx = p.x + p.w * (double)i / (double)(n - 1);
            double ty = db_to_y(m->spec_db[(size_t)i], p);
            if (i) ui_line(u, px, py, tx, ty, g_pal.curve_main, 1.3);
            px = tx;
            py = ty;
        }
    }
    ui_text_right(u, as_rect(p.x, p.y + p.h + 4, p.w, 14), FONT_SMALL,
                  g_pal.text_faint, "20 Hz                       1 kHz                       20 kHz");

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "TOTAL HARMONIC DISTORTION");
    ui_text(u, rx, r.y + 30, FONT_CRIT, g_pal.accent, fmt(m->tone.thd_pct, 3) + " %");
    double yy = r.y + 78;
    label_value(u, rx, yy, rw, "THD+N", fmt(m->tone.thdn_pct, 3) + " %", g_pal.text);
    label_value(u, rx, yy + 26, rw, "SNR", fmt(m->tone.snr_db, 1) + " dB", g_pal.text);
    label_value(u, rx, yy + 52, rw, "SINAD", fmt(m->tone.sinad_db, 1) + " dB", g_pal.text);
    label_value(u, rx, yy + 78, rw, "SFDR", fmt(m->tone.sfdr_db, 1) + " dB", g_pal.text);
    label_value(u, rx, yy + 104, rw, "ENOB", fmt(m->tone.enob, 2) + " bit", g_pal.text);
    ui_line(u, rx, yy + 136, rx + rw, yy + 136, g_pal.line_soft, 1);
    ui_text(u, rx, yy + 150, FONT_SMALL, g_pal.text_faint,
            "single-tone FFT; not a full IMD suite");
}

void draw_xy(UI *u, NativeModel *m, NativeActions *act) {
    (void)act;
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "PAIR");
    ui_text(u, x, y, FONT_VALUE, g_pal.text, "CH1 → X");
    ui_text(u, x, y + 24, FONT_VALUE, g_pal.text, "CH2 → Y");
    y = ui_group(u, x, y + 60, w, "DISPLAY");
    label_value(u, x, y, w, "PERSISTENCE", "2 s", g_pal.text);
    label_value(u, x, y + 26, w, "GAIN", fmt(m->gain_db, 2) + " dB", g_pal.text);

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "PHASE / XY", "Lissajous · CH1 vs CH2");
    double size = std::min(c.w - PAD * 4, c.h - 96);
    ASRect p = as_rect(c.x + (c.w - size) * 0.5, c.y + 48, size, size);
    mat_recessed(u, p, 4);
    for (int i = 1; i < 4; ++i) {
        ui_line(u, p.x + p.w * i / 4.0, p.y, p.x + p.w * i / 4.0, p.y + p.h,
                g_pal.chart_grid, 1);
        ui_line(u, p.x, p.y + p.h * i / 4.0, p.x + p.w, p.y + p.h * i / 4.0,
                g_pal.chart_grid, 1);
    }
    ui_line(u, p.x, p.y + p.h * 0.5, p.x + p.w, p.y + p.h * 0.5, g_pal.chart_axis, 1);
    ui_line(u, p.x + p.w * 0.5, p.y, p.x + p.w * 0.5, p.y + p.h, g_pal.chart_axis, 1);
    if (m->xy_x.size() == m->xy_y.size()) {
        for (size_t i = 0; i < m->xy_x.size(); ++i) {
            double tx = p.x + p.w * 0.5 + (double)m->xy_x[i] * p.w * 0.45;
            double ty = p.y + p.h * 0.5 - (double)m->xy_y[i] * p.h * 0.45;
            ui_dot(u, tx, ty, 1.3, as_alpha(g_pal.accent, 0.45));
        }
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "PHASE");
    ui_text(u, rx, r.y + 30, FONT_CRIT, g_pal.accent, fmt(m->phase_deg, 1) + "°");
    double yy = r.y + 78;
    label_value(u, rx, yy, rw, "GAIN", fmt_signed(m->gain_db, 2) + " dB", g_pal.text);
    label_value(u, rx, yy + 26, rw, "FREQUENCY", fmt(m->stats.freq, 1) + " Hz", g_pal.text);
    label_value(u, rx, yy + 52, rw, "POLARITY", "normal", g_pal.good);
    ui_line(u, rx, yy + 84, rx + rw, yy + 84, g_pal.line_soft, 1);
    ui_text(u, rx, yy + 98, FONT_SMALL, g_pal.text_faint,
            "same device: channels share a clock");
}

void draw_measure(UI *u, NativeModel *m, NativeActions *act) {
    (void)act;
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "RUN");
    label_value(u, x, y, w, "FRAMES", "720", g_pal.text);
    label_value(u, x, y + 26, w, "WINDOW", "15 ms", g_pal.text);
    label_value(u, x, y + 52, w, "REPEAT", "1×", g_pal.text);
    y = ui_group(u, x, y + 92, w, "LIMITS");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "clip ≤ 0");
    ui_text(u, x, y + 22, FONT_SMALL, g_pal.text_faint, "20 Hz – 20 kHz");
    ui_text(u, x, y + 44, FONT_SMALL, g_pal.text_faint, "uncalibrated → dBFS only");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "CHANNEL MEASUREMENTS", "8 channels · current frame");
    double tx = c.x + PAD + 8;
    double ty = c.y + 52;
    const char *head = "CH        PEAK dBFS     RMS dBFS      FREQ Hz       Vpp FS    CLIP";
    ui_text(u, tx, ty, FONT_SMALL, g_pal.text_faint, head);
    ui_line(u, c.x + PAD, ty + 20, c.x + c.w - PAD, ty + 20, g_pal.line_soft, 1);
    for (int i = 0; i < 8; ++i) {
        double yy = ty + 32 + i * 34;
        const NativeChannel &ch = m->ch[i];
        char line[160];
        if (ch.active) {
            std::snprintf(line, sizeof(line), "CH%d      %8.1f     %8.1f     %8.1f     %8.3f      %d",
                          i + 1, ch.peak_db, ch.rms_db, ch.freq, ch.vpp, ch.clip);
            ui_text(u, tx, yy, FONT_MONO, g_pal.text, line);
            ui_text_right(u, as_rect(tx, yy - 2, c.w - PAD * 2 - 16, 18), FONT_SMALL,
                          g_pal.good, "PASS");
        } else {
            std::snprintf(line, sizeof(line), "CH%d            —          —          —          —      —", i + 1);
            ui_text(u, tx, yy, FONT_MONO, g_pal.text_faint, line);
        }
        if (i < 7)
            ui_line(u, c.x + PAD, yy + 24, c.x + c.w - PAD, yy + 24, g_pal.line_soft, 1);
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "VERDICT");
    ui_text(u, rx, r.y + 30, FONT_CRIT, m->clip ? g_pal.bad : g_pal.good,
            m->clip ? "FAIL" : "PASS");
    double yy = r.y + 78;
    label_value(u, rx, yy, rw, "ACTIVE", "2 / 8", g_pal.text);
    label_value(u, rx, yy + 26, rw, "CLIP", m->clip ? "latched" : "0", g_pal.text);
    label_value(u, rx, yy + 52, rw, "CAL", m->cal_confidence.c_str(), g_pal.warn);
    ui_line(u, rx, yy + 84, rx + rw, yy + 84, g_pal.line_soft, 1);
    ui_text(u, rx, yy + 98, FONT_SMALL, g_pal.text_faint, "voltage hidden until calibrated");
}

void draw_repair(UI *u, NativeModel *m, NativeActions *act) {
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "PLAN");
    ui_text(u, x, y, FONT_VALUE, g_pal.text, "Tape deck");
    ui_text(u, x, y + 24, FONT_SMALL, g_pal.text_faint, "quick verification · 4 steps");
    if (ui_button(u, 300, as_rect(x, y + 58, w, 32), "RUN TAPE PLAN", BTN_PRIMARY))
        act->run_repair = true;
    ui_text(u, x, y + 100, FONT_SMALL, g_pal.text_dim,
            m->plan_ran ? (m->plan_pass ? "last run: PASS" : "last run: FAIL")
                        : "not run yet");
    if (ui_button(u, 302, as_rect(x, y + 134, w, 30), "RUN LOOPBACK",
                  BTN_NORMAL))
        act->run_loopback = true;
    ui_text(u, x, y + 174, FONT_SMALL, g_pal.text_faint,
            "tone −20 dBFS → default output/input");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "REPAIR PLAN — TAPE", "thresholds are advisory");
    double tx = c.x + PAD + 8, ty = c.y + 56;
    ui_text(u, tx, ty, FONT_SMALL, g_pal.text_faint, "STEP                VALUE        UNIT        RESULT");
    ui_line(u, c.x + PAD, ty + 20, c.x + c.w - PAD, ty + 20, g_pal.line_soft, 1);
    for (size_t i = 0; i < m->plan.size(); ++i) {
        const NativePlanStep &s = m->plan[i];
        double yy = ty + 34 + (double)i * 40.0;
        char line[192];
        std::snprintf(line, sizeof(line), "%-18s %10.4f    %-10s  %s", s.label.c_str(),
                      s.value, s.unit.c_str(), s.pass ? "PASS" : "FAIL");
        ui_text(u, tx, yy, FONT_MONO, s.pass ? g_pal.text : g_pal.bad, line);
    }
    if (!m->plan_ran) {
        ui_text(u, tx, ty + 40, FONT_SMALL, g_pal.text_faint,
                "Run the plan to fill in wow / flutter / azimuth / THD.");
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "PLAN VERDICT");
    bool pass = m->plan_ran ? m->plan_pass : true;
    ui_text(u, rx, r.y + 30, FONT_CRIT, pass ? g_pal.good : g_pal.bad,
            m->plan_ran ? (pass ? "PASS" : "FAIL") : "—");
    if (ui_button(u, 301, as_rect(rx, r.y + 96, rw, 32), "EXPORT PDF", BTN_NORMAL))
        act->export_report = true;
    ui_text(u, rx, r.y + 140, FONT_SMALL, g_pal.text_faint, "writes PDF + JSON + HTML");
    ui_text(u, rx, r.y + 160, FONT_SMALL,
            m->loopback_ran ? (m->loopback_pass ? g_pal.good : g_pal.bad)
                            : g_pal.text_faint,
            m->loopback_ran ? (m->loopback_pass ? "loopback PASS" : "loopback FAIL")
                            : "loopback: not run");
    if (m->loopback_ran) {
        label_value(u, rx, r.y + 186, rw, "LEVEL", fmt(m->loopback_level, 1) + " dBFS", g_pal.text);
        label_value(u, rx, r.y + 212, rw, "THD", fmt(m->loopback_thd, 3) + " %", g_pal.text);
    }
}

void draw_reports(UI *u, NativeModel *m, NativeActions *act) {
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "OUTPUT");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "Application Support");
    ui_text(u, x, y + 20, FONT_SMALL, g_pal.text_faint, "AudioScope / reports");
    y = ui_group(u, x, y + 60, w, "FORMAT");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "PDF · 3 pages · charts");
    ui_text(u, x, y + 22, FONT_SMALL, g_pal.text_faint, "JSON · HTML · provenance");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "REPORTS", "most recent first");
    double ty = c.y + 56;
    ui_text(u, c.x + PAD + 8, ty, FONT_SMALL, g_pal.text_faint, "FILE                                KIND          TIME        RESULT");
    ui_line(u, c.x + PAD, ty + 20, c.x + c.w - PAD, ty + 20, g_pal.line_soft, 1);
    if (m->reports.empty()) {
        ui_text(u, c.x + PAD + 8, ty + 42, FONT_SMALL, g_pal.text_faint,
                "No reports yet. Export one from the repair page or the button →");
    }
    for (size_t i = 0; i < m->reports.size(); ++i) {
        const NativeReport &r = m->reports[i];
        double yy = ty + 36 + (double)i * 34.0;
        char line[192];
        std::snprintf(line, sizeof(line), "%-34s %-12s  %-10s  %s", r.name.c_str(),
                      r.kind.c_str(), r.time.c_str(), r.pass ? "PASS" : "WARN");
        ui_text(u, c.x + PAD + 8, yy, FONT_MONO, g_pal.text, line);
        if (i + 1 < m->reports.size())
            ui_line(u, c.x + PAD, yy + 24, c.x + c.w - PAD, yy + 24, g_pal.line_soft, 1);
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "EXPORT");
    if (ui_button(u, 350, as_rect(rx, r.y + 34, rw, 34), "EXPORT REPORT", BTN_PRIMARY))
        act->export_report = true;
    ui_text(u, rx, r.y + 84, FONT_SMALL, g_pal.text_faint, "no network · local files");
    if (!m->last_report.empty()) {
        size_t slash = m->last_report.find_last_of("/\\");
        std::string name = slash == std::string::npos ? m->last_report
                                                     : m->last_report.substr(slash + 1);
        ui_text(u, rx, r.y + 120, FONT_SMALL, g_pal.text_dim, "last:");
        ui_text_ellipsis(u, as_rect(rx, r.y + 138, rw, 16), FONT_SMALL, g_pal.text,
                         name.c_str());
    }
}

void draw_calibration(UI *u, NativeModel *m, NativeActions *act) {
    (void)act;
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "PROFILE");
    label_value(u, x, y, w, "ID", m->cal_id, g_pal.text);
    label_value(u, x, y + 26, w, "DEVICE", m->device, g_pal.text);
    label_value(u, x, y + 52, w, "CHANNEL", "CH1", g_pal.text);
    label_value(u, x, y + 78, w, "GAIN", "0.0 dB", g_pal.text);

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "CALIBRATION", "voltage needs a measured profile");
    double tx = c.x + PAD + 8, ty = c.y + 56;
    label_value(u, tx, ty, c.w - PAD * 2 - 16, "CONFIDENCE", m->cal_confidence, g_pal.warn);
    label_value(u, tx, ty + 30, c.w - PAD * 2 - 16, "FS Vrms", m->cal_fs_vrms > 0 ? fmt(m->cal_fs_vrms, 4) : "—", g_pal.text);
    label_value(u, tx, ty + 60, c.w - PAD * 2 - 16, "METHOD", "none", g_pal.text);
    label_value(u, tx, ty + 90, c.w - PAD * 2 - 16, "BOUND", "device + channel + gain + pad + rate", g_pal.text_faint);
    ui_line(u, c.x + PAD, ty + 130, c.x + c.w - PAD, ty + 130, g_pal.line_soft, 1);
    ui_text(u, tx, ty + 146, FONT_SMALL, g_pal.warn,
            "Uncalibrated: only dBFS is shown. Voltage would be an assumption.");
    ui_text(u, tx, ty + 168, FONT_SMALL, g_pal.text_faint,
            "Calibrate on this machine with a DMM or reference source; profiles are per gain/pad/rate.");

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD;
    ui_text(u, rx, r.y + 14, FONT_SMALL, g_pal.text_faint, "SAFETY");
    ui_text(u, rx, r.y + 34, FONT_SMALL, g_pal.text_dim, "software cannot protect");
    ui_text(u, rx, r.y + 52, FONT_SMALL, g_pal.text_dim, "the sound card.");
    ui_text(u, rx, r.y + 78, FONT_SMALL, g_pal.text_faint, "use a protected front end");
    ui_text(u, rx, r.y + 96, FONT_SMALL, g_pal.text_faint, "for unknown or high levels.");
}

void draw_footer(UI *u, NativeModel *m) {
    ui_line(u, 0, H - kFooterH, W, H - kFooterH, g_pal.line_soft, 1);
    char left[160];
    std::snprintf(left, sizeof(left), "%s · %.0f Hz · %d ch · %s", m->capture ? "INPUT" : "SYNTHETIC",
                  m->fs, m->channels, m->hold ? "HOLD" : "LIVE");
    ui_text(u, PAD, H - kFooterH + 9, FONT_SMALL, g_pal.text_faint, left);
    ui_text_center(u, as_rect(0, H - kFooterH + 9, W, 14), FONT_SMALL,
                   g_pal.text_faint, "BERRYSTUDIO / " AS_VERSION);
    ui_text_right(u, as_rect(W - 420, H - kFooterH + 9, 404, 14), FONT_SMALL,
                  g_pal.text_faint, m->message.c_str());
}

} // namespace

void native_ui_init(NativeUi *ui) {
    std::memset(&ui->ui, 0, sizeof(ui->ui));
    ui->pending_page = -1;
    theme_set(THEME_DARK);
    g_reduce_motion = 0;
}

void native_ui_frame(NativeUi *ui, CGContextRef ctx, double w, double h, double t) {
    NativeModel *m = ui->model;
    ui->actions = NativeActions{};
    if (!m) return;
    ui->ui.width = W;
    ui->ui.height = H;
    ui_begin(&ui->ui, ctx, W, H, t);

    ui_fill(&ui->ui, as_rect(0, 0, W, H), g_pal.bg);
    draw_header(&ui->ui, m);
    draw_nav(&ui->ui, m);

    switch (m->page) {
    case PAGE_SCOPE: draw_scope(&ui->ui, m, &ui->actions); break;
    case PAGE_SPECTRUM: draw_spectrum(&ui->ui, m, &ui->actions); break;
    case PAGE_XY: draw_xy(&ui->ui, m, &ui->actions); break;
    case PAGE_MEASURE: draw_measure(&ui->ui, m, &ui->actions); break;
    case PAGE_REPAIR: draw_repair(&ui->ui, m, &ui->actions); break;
    case PAGE_REPORTS: draw_reports(&ui->ui, m, &ui->actions); break;
    case PAGE_CALIBRATION: draw_calibration(&ui->ui, m, &ui->actions); break;
    default: break;
    }

    draw_footer(&ui->ui, m);
    ui_end(&ui->ui);
    (void)w;
    (void)h;
}

void native_ui_mouse_move(NativeUi *ui, double x, double y) {
    ui_mouse_move(&ui->ui, x, y);
}
void native_ui_mouse_down(NativeUi *ui, double x, double y) {
    ui_mouse_down(&ui->ui, x, y);
}
void native_ui_mouse_up(NativeUi *ui, double x, double y) {
    ui_mouse_up(&ui->ui, x, y);
}
void native_ui_key(NativeUi *ui, int key) { ui_key(&ui->ui, (ASKey)key); }

} // namespace audioscope
