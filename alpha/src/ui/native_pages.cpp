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

/* 数字格式：真减号 U+2212（BERRY-UI-SPEC §3.5）。 */
std::string minus_sign(std::string s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '-') out += "\xe2\x88\x92";
        else out.push_back(c);
    }
    return out;
}

std::string fmt(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return minus_sign(buf);
}

std::string fmt_signed(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.*f", decimals, v);
    return minus_sign(buf);
}

/* ---- 0.9.3: device + preset helpers ---------------------------------- */

std::string device_name(const NativeModel *m, int sel, bool input) {
    if (!m || sel < 0 || sel >= (int)m->devices.size()) return "system default";
    const NativeDevice &d = m->devices[(size_t)sel];
    if (input ? !d.is_input : !d.is_output) return "system default";
    return d.name;
}

int cycle_device(const NativeModel *m, int sel, bool input) {
    if (!m) return -1;
    std::vector<int> cand;
    cand.push_back(-1);
    for (size_t i = 0; i < m->devices.size(); ++i) {
        const NativeDevice &d = m->devices[i];
        if (input ? d.is_input : d.is_output) cand.push_back((int)i);
    }
    for (size_t k = 0; k < cand.size(); ++k) {
        if (cand[k] == sel) return cand[(k + 1) % cand.size()];
    }
    return cand.size() > 1 ? cand[1] : cand[0];
}

int preset_index(double value, const double *values, int n) {
    int best = 0;
    for (int i = 1; i < n; ++i)
        if (std::fabs(values[i] - value) < std::fabs(values[best] - value)) best = i;
    return best;
}

void label_value(UI *u, double x, double y, double w, const char *label,
                 const std::string &value, uint32_t color) {
    /* 同一基线上混排 Label(11) 与 Value(16)：行高不同也不会上下错位，
     * 右侧数值形成一条干净的列。 */
    double baseline = y + ui_font_height(FONT_VALUE) * 0.78;
    ui_text_baseline(u, x, baseline, FONT_SMALL, g_pal.text_faint, label);
    ui_text_baseline_right(u, x + w, baseline, FONT_VALUE, color,
                           value.c_str());
}

void title_block(UI *u, ASRect r, const char *title, const char *sub) {
    double baseline = r.y + 22;
    ui_text_baseline(u, r.x + PAD, baseline, FONT_LABEL, g_pal.text_dim, title);
    if (sub && *sub)
        ui_text_baseline_right(u, r.x + r.w - PAD, baseline, FONT_SMALL,
                               g_pal.text_faint, sub);
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

/* --------------------------------------------------------------- header */
void draw_header(UI *u, NativeModel *m) {
    ui_fill(u, as_rect(0, 0, W, kHeaderH), g_pal.bg);
    /* 品牌：标记 + 字标 + 机构/版本副行。字标是签名，不是 H1。 */
    ui_dot(u, 30, 32, 9, g_pal.accent);
    ui_dot(u, 30, 32, 3, g_pal.bg);
    ui_text_baseline(u, 48, 29, FONT_BRAND, g_pal.text, "AUDIOSCOPE");
    ui_text_baseline(u, 48, 45, FONT_MONO, g_pal.text_faint,
                     "BERRYSTUDIO / " AS_VERSION);
    ui_text_baseline_right(u, W - 272, 29, FONT_SMALL,
                           m->capture ? g_pal.warn : g_pal.good,
                           m->capture ? "CAPTURE · INPUT" : "SYNTHETIC SOURCE");
    ui_text_baseline_right(u, W - 272, 45, FONT_SMALL, g_pal.text_faint,
                           m->device.c_str());
    if (ui_button(u, 900, as_rect(W - 152, 17, 64, 30),
                  m->light ? "LIGHT" : "DARK", BTN_GHOST)) {
        theme_toggle();
        m->light = theme_is_light();
    }
    if (ui_button(u, 901, as_rect(W - 80, 17, 64, 30), "ABOUT", BTN_GHOST)) {
        m->message = AS_BRAND " " AS_VERSION;
    }
    ui_line(u, 0, kHeaderH - 1, W, kHeaderH - 1, g_pal.line_soft, 1);
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
    label_value(u, x, y + 26, w, "IN", device_name(m, m->sel_in, true),
                g_pal.text);
    if (ui_button(u, 202, as_rect(x, y + 46, w, 26), "CYCLE INPUT", BTN_SMALL)) {
        m->sel_in = cycle_device(m, m->sel_in, true);
        m->io_dirty = true;
    }
    label_value(u, x, y + 80, w, "OUT", device_name(m, m->sel_out, false),
                g_pal.text);
    if (ui_button(u, 203, as_rect(x, y + 100, w, 26), "CYCLE OUTPUT", BTN_SMALL)) {
        m->sel_out = cycle_device(m, m->sel_out, false);
        m->io_dirty = true;
    }
    label_value(u, x, y + 134, w, "RATE", fmt(m->fs, 0) + " Hz", g_pal.text);
    label_value(u, x, y + 158, w, "MODE", "Inst", g_pal.text);
    if (ui_button(u, 200, as_rect(x, y + 184, w, 30),
                  m->capture ? "STOP CAPTURE" : "CAPTURE INPUT",
                  m->capture ? BTN_DANGER : BTN_NORMAL)) {
        act->toggle_capture = true;
    }
    int hold = m->hold ? 1 : 0;
    if (ui_toggle(u, 201, as_rect(x, y + 222, w, 30), "HOLD  (freeze)", &hold)) {
        m->hold = hold != 0;
        m->message = m->hold ? "display held" : "display live";
    }

    y = ui_group(u, x, y + 274, w, "CHANNELS");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "CH1  ·  AC  ·  ±1.0 V");
    ui_text(u, x, y + 22, FONT_SMALL, g_pal.text_faint, "CH2  ·  AC  ·  ±1.0 V");

    y = ui_group(u, x, y + 58, w, "TIMEBASE");
    ui_text(u, x, y, FONT_VALUE, g_pal.text, "1.50 ms/div");
    ui_text_baseline(u, x, y + 34, FONT_SMALL, g_pal.text_faint,
                     "trigger: CH1 rising · 0.00 V");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    const double win_ms = 720.0 / (m->fs > 0 ? m->fs : 48000.0) * 1000.0;
    char scope_sub[96];
    std::snprintf(scope_sub, sizeof(scope_sub), "CH1 · %.0f Hz · %.2f ms window",
                  m->fs, win_ms);
    title_block(u, c, "TIME DOMAIN", scope_sub);
    /* 左侧留 34px 给 Y 轴刻度：刻度不压曲线（图表安全区规则）。 */
    ASRect plot = as_rect(c.x + PAD + 34, c.y + 48, c.w - PAD * 2 - 44, c.h - 96);
    ui_plot(u, plot, 4);
    plot_grid(u, plot, 10, 8);
    double mid = plot.y + plot.h * 0.5;
    ui_line(u, plot.x, mid, plot.x + plot.w, mid, g_pal.chart_axis, 1);

    if (m->wave.size() > 1) {
        int n = (int)m->wave.size();
        ui_polyline_f(u, plot.x, plot.w / (double)(n - 1), m->wave.data(), n,
                      mid, plot.h * 0.44, g_pal.curve_main, 1.4);
    }
    double dash[2] = {4, 4};
    ui_line_dash(u, plot.x + plot.w * 0.5, plot.y, plot.x + plot.w * 0.5,
                 plot.y + plot.h, g_pal.warn, 1, dash, 2);

    /* Y 轴：±1.0 FS / 0；X 轴：0 / 7.5 / 15 ms。 */
    ui_text_baseline_right(u, plot.x - 6, plot.y + 12, FONT_SMALL,
                           g_pal.text_faint, "+1.0");
    ui_text_baseline_right(u, plot.x - 6, mid + 4, FONT_SMALL,
                           g_pal.text_faint, "0");
    ui_text_baseline_right(u, plot.x - 6, plot.y + plot.h - 2, FONT_SMALL,
                           g_pal.text_faint, "\xe2\x88\x92" "1.0");
    double bottom = plot.y + plot.h + 18;
    ui_text_baseline(u, plot.x, bottom, FONT_SMALL, g_pal.text_faint, "0");
    char midbuf[32];
    std::snprintf(midbuf, sizeof(midbuf), "%.1f ms", win_ms * 0.5);
    const char *midlab = midbuf;
    double mw = ui_text_width(u, FONT_SMALL, midlab);
    ui_text_baseline(u, plot.x + plot.w * 0.5 - mw * 0.5, bottom, FONT_SMALL,
                     g_pal.text_faint, midlab);
    char endbuf[32];
    std::snprintf(endbuf, sizeof(endbuf), "%.1f ms", win_ms);
    ui_text_baseline_right(u, plot.x + plot.w, bottom, FONT_SMALL,
                           g_pal.text_faint, endbuf);

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD;
    double rw = r.w - PAD * 2;
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint,
                     "INPUT PEAK · CH1");
    ui_text_baseline(u, rx, r.y + 54, FONT_CRIT, g_pal.accent,
                     (fmt(as_linear_to_db(m->stats.peak), 1) + " dBFS").c_str());
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

    /* 常驻安全声明：设备选择变多了，这条不能丢。 */
    const double sy = r.y + r.h - 96;
    ui_line(u, rx, sy - 12, rx + rw, sy - 12, g_pal.line_soft, 1);
    ui_text_baseline(u, rx, sy + 6, FONT_LABEL, g_pal.text_faint, "SAFETY");
    ui_text_baseline(u, rx, sy + 28, FONT_SMALL, g_pal.text_dim,
                     "software cannot protect");
    ui_text_baseline(u, rx, sy + 46, FONT_SMALL, g_pal.text_dim,
                     "the sound card.");
    ui_text_baseline(u, rx, sy + 66, FONT_SMALL, g_pal.text_faint,
                     "use a protected front end");
}

void draw_spectrum(UI *u, NativeModel *m, NativeActions *act) {
    (void)act;
    ASRect l = left_card();
    mat_raised(u, l, RADIUS_CARD);
    double x = l.x + PAD, w = l.w - PAD * 2;
    double y = ui_group(u, x, l.y + PAD, w, "FFT");
    static const char *const kFftSizes[5] = {"1024", "2048", "4096", "8192",
                                              "16384"};
    static const int kFftValues[5] = {1024, 2048, 4096, 8192, 16384};
    int fft_sel = 2;
    for (int i = 0; i < 5; ++i)
        if (kFftValues[i] == m->fft_size) fft_sel = i;
    if (ui_cycle(u, 230, as_rect(x, y, w, 26), kFftSizes, 5, &fft_sel))
        m->fft_size = kFftValues[fft_sel];
    label_value(u, x, y + 34, w, "WINDOW", "Hann", g_pal.text);
    label_value(u, x, y + 60, w, "RATE", fmt(m->fs, 0) + " Hz", g_pal.text);
    y = ui_group(u, x, y + 100, w, "SOURCE");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, m->capture ? "capture · CH1" : "synthetic · CH1");
    ui_text(u, x, y + 24, FONT_SMALL, g_pal.text_faint, "peak-relative, 100 dB floor");
    y = ui_group(u, x, y + 62, w, "HARMONICS");
    ui_text(u, x, y, FONT_SMALL, g_pal.text_dim, "H2  −58.1 dB");
    ui_text(u, x, y + 22, FONT_SMALL, g_pal.text_faint, "H3  −62.4 dB");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    const std::string sub =
        "Hann · " + std::to_string(m->fft_size) + " · 20 Hz – 20 kHz";
    title_block(u, c, "SPECTRUM", sub.c_str());
    ASRect p = as_rect(c.x + PAD + 40, c.y + 48, c.w - PAD * 2 - 56, c.h - 96);
    ui_plot(u, p, 4);
    for (int i = 0; i <= 5; ++i)
        ui_line(u, p.x, p.y + p.h * (double)i / 5.0, p.x + p.w,
                p.y + p.h * (double)i / 5.0, g_pal.chart_grid, 1);
    for (int i = 0; i <= 4; ++i)
        ui_line(u, p.x + p.w * (double)i / 4.0, p.y, p.x + p.w * (double)i / 4.0,
                p.y + p.h, g_pal.chart_grid, 1);

    if (m->spec_db.size() > 1) {
        int n = (int)m->spec_db.size();
        /* 0 dBFS 在顶部、−100 dB 在底部：y = p.y − db × (p.h/100) */
        ui_polyline_d(u, p.x, p.w / (double)(n - 1), m->spec_db.data(), n,
                      p.y, p.h * 0.01, g_pal.curve_main, 1.3);
    }
    /* Y 轴 dB 刻度（0 / −20 … −100）放在左侧安全区外。 */
    for (int i = 0; i <= 5; ++i) {
        char lab[16];
        if (i == 0) std::snprintf(lab, sizeof(lab), "0");
        else std::snprintf(lab, sizeof(lab), "\xe2\x88\x92%d", i * 20);
        ui_text_baseline_right(u, p.x - 6, p.y + p.h * (double)i / 5.0 + 4,
                               FONT_SMALL, g_pal.text_faint, lab);
    }
    /* X 轴三个有意义的点：20 Hz / 1 kHz / 20 kHz。 */
    double bottom = p.y + p.h + 18;
    ui_text_baseline(u, p.x, bottom, FONT_SMALL, g_pal.text_faint, "20 Hz");
    const char *midlab = "1 kHz";
    double mw = ui_text_width(u, FONT_SMALL, midlab);
    ui_text_baseline(u, p.x + p.w * 0.5 - mw * 0.5, bottom, FONT_SMALL,
                     g_pal.text_faint, midlab);
    ui_text_baseline_right(u, p.x + p.w, bottom, FONT_SMALL, g_pal.text_faint,
                           "20 kHz");

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint,
                     "TOTAL HARMONIC DISTORTION");
    ui_text_baseline(u, rx, r.y + 54, FONT_CRIT, g_pal.accent,
                     (fmt(m->tone.thd_pct, 3) + " %").c_str());
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
    ui_plot(u, p, 4);
    for (int i = 1; i < 4; ++i) {
        ui_line(u, p.x + p.w * i / 4.0, p.y, p.x + p.w * i / 4.0, p.y + p.h,
                g_pal.chart_grid, 1);
        ui_line(u, p.x, p.y + p.h * i / 4.0, p.x + p.w, p.y + p.h * i / 4.0,
                g_pal.chart_grid, 1);
    }
    ui_line(u, p.x, p.y + p.h * 0.5, p.x + p.w, p.y + p.h * 0.5, g_pal.chart_axis, 1);
    ui_line(u, p.x + p.w * 0.5, p.y, p.x + p.w * 0.5, p.y + p.h, g_pal.chart_axis, 1);
    if (m->xy_x.size() == m->xy_y.size() && m->xy_x.size() > 1) {
        /* 连通轨迹比 512 个独立圆点快得多，Lissajous 的形状也更连续。 */
        ui_path_xy_f(u, m->xy_x.data(), m->xy_y.data(), (int)m->xy_x.size(),
                     p.x + p.w * 0.5, p.y + p.h * 0.5, p.w * 0.45,
                     p.h * 0.45, as_alpha(g_pal.accent, 0.55), 1.1);
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint, "PHASE");
    ui_text_baseline(u, rx, r.y + 54, FONT_CRIT, g_pal.accent,
                     (fmt(m->phase_deg, 1) + "°").c_str());
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

    /* ---- 0.9.3 signal generator：所有输出都从 −20 dBFS 起 ---------- */
    y = ui_group(u, x, y + 76, w, "GENERATOR");
    const char *wave_labels[AS_WAVE_COUNT];
    for (int i = 0; i < AS_WAVE_COUNT; ++i) wave_labels[i] = as_wave_name(i);
    int wave = m->gen_wave >= 0 && m->gen_wave < AS_WAVE_COUNT ? m->gen_wave : 0;
    ui_text_baseline(u, x, y + 18, FONT_SMALL, g_pal.text_faint, "WAVE");
    if (ui_cycle(u, 240, as_rect(x + 44, y, w - 44, 26), wave_labels,
                 AS_WAVE_COUNT, &wave)) {
        m->gen_wave = wave;
        m->gen_dirty = true;
    }
    ui_text_baseline(u, x, y + 48, FONT_SMALL, g_pal.text_faint, "FREQ");
    static const double kGenFreq[7] = {20, 100, 440, 1000, 5000, 10000, 20000};
    static const char *const kGenFreqLabels[7] = {"20 Hz", "100 Hz", "440 Hz",
                                                   "1 kHz", "5 kHz", "10 kHz",
                                                   "20 kHz"};
    int freq_idx = preset_index(m->gen_hz, kGenFreq, 7);
    if (ui_cycle(u, 242, as_rect(x + 44, y + 30, w - 44, 26), kGenFreqLabels, 7,
                 &freq_idx)) {
        m->gen_hz = kGenFreq[freq_idx];
        m->gen_dirty = true;
    }
    ui_text_baseline(u, x, y + 78, FONT_SMALL, g_pal.text_faint, "LEVEL");
    static const double kGenLevel[4] = {-60, -40, -30, -20};
    static const char *const kGenLevelLabels[4] = {"−60 dBFS", "−40 dBFS",
                                                    "−30 dBFS", "−20 dBFS"};
    int level_idx = preset_index(m->gen_db, kGenLevel, 4);
    if (ui_cycle(u, 244, as_rect(x + 44, y + 60, w - 44, 26), kGenLevelLabels, 4,
                 &level_idx)) {
        m->gen_db = kGenLevel[level_idx];
        m->gen_dirty = true;
    }
    ui_text_baseline(u, x, y + 108, FONT_SMALL, g_pal.text_faint, "PERIOD");
    static const double kGenPeriod[5] = {0.05, 0.1, 0.5, 1.0, 2.0};
    static const char *const kGenPeriodLabels[5] = {"50 ms", "100 ms", "500 ms",
                                                     "1 s", "2 s"};
    int period_idx = preset_index(m->gen_period, kGenPeriod, 5);
    if (ui_cycle(u, 246, as_rect(x + 44, y + 90, w - 44, 26), kGenPeriodLabels,
                 5, &period_idx)) {
        m->gen_period = kGenPeriod[period_idx];
        m->gen_dirty = true;
    }
    int gen_on = m->gen_on ? 1 : 0;
    if (ui_toggle(u, 248, as_rect(x, y + 128, w, 30),
                  m->gen_on ? "OUTPUT ON" : "OUTPUT OFF", &gen_on)) {
        m->gen_on = gen_on != 0;
        m->gen_dirty = true;
    }
    ui_text(u, x, y + 168, FONT_SMALL, g_pal.text_faint,
            m->gen_hot_allowed ? "hot unlocked" : "locked ≤ −20 dBFS");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "CHANNEL MEASUREMENTS", "8 channels · current frame");
    /* 表格用固定列坐标，表头与数值共用一条右对齐基线：比例字体表头和等宽
     * 数值不会再错位（0.9 之前表头是拼空格拼出来的）。 */
    double tx = c.x + PAD + 8;
    double tw = c.w - PAD * 2 - 16;
    double right = c.x + c.w - PAD;
    double ty = c.y + 52;
    double hb = ty + 11;
    const double col_peak = tx + tw * 0.22;
    const double col_rms = tx + tw * 0.40;
    const double col_freq = tx + tw * 0.60;
    const double col_vpp = tx + tw * 0.76;
    const double col_clip = tx + tw * 0.87;
    ui_text_baseline(u, tx, hb, FONT_SMALL, g_pal.text_faint, "CH");
    ui_text_baseline_right(u, col_peak, hb, FONT_SMALL, g_pal.text_faint,
                           "PEAK dBFS");
    ui_text_baseline_right(u, col_rms, hb, FONT_SMALL, g_pal.text_faint,
                           "RMS dBFS");
    ui_text_baseline_right(u, col_freq, hb, FONT_SMALL, g_pal.text_faint,
                           "FREQ Hz");
    ui_text_baseline_right(u, col_vpp, hb, FONT_SMALL, g_pal.text_faint,
                           "Vpp FS");
    ui_text_baseline_right(u, col_clip, hb, FONT_SMALL, g_pal.text_faint,
                           "CLIP");
    ui_line(u, c.x + PAD, ty + 20, right, ty + 20, g_pal.line_soft, 1);
    for (int i = 0; i < 8; ++i) {
        double yy = ty + 32 + i * 34;
        double rb = yy + 12;
        const NativeChannel &ch = m->ch[i];
        std::string name = "CH" + std::to_string(i + 1);
        const uint32_t col = ch.active ? g_pal.text : g_pal.text_faint;
        ui_text_baseline(u, tx, rb, FONT_MONO, col, name.c_str());
        if (ch.active) {
            ui_text_baseline_right(u, col_peak, rb, FONT_MONO, g_pal.text,
                                   fmt(ch.peak_db, 1).c_str());
            ui_text_baseline_right(u, col_rms, rb, FONT_MONO, g_pal.text,
                                   fmt(ch.rms_db, 1).c_str());
            ui_text_baseline_right(u, col_freq, rb, FONT_MONO, g_pal.text,
                                   fmt(ch.freq, 1).c_str());
            ui_text_baseline_right(u, col_vpp, rb, FONT_MONO, g_pal.text,
                                   fmt(ch.vpp, 3).c_str());
            ui_text_baseline_right(u, col_clip, rb, FONT_MONO, g_pal.text,
                                   std::to_string(ch.clip).c_str());
            ui_text_baseline_right(u, right, rb, FONT_SMALL, g_pal.good, "PASS");
        } else {
            ui_text_baseline_right(u, col_peak, rb, FONT_MONO, g_pal.text_faint, "—");
            ui_text_baseline_right(u, col_rms, rb, FONT_MONO, g_pal.text_faint, "—");
            ui_text_baseline_right(u, col_freq, rb, FONT_MONO, g_pal.text_faint, "—");
            ui_text_baseline_right(u, col_vpp, rb, FONT_MONO, g_pal.text_faint, "—");
            ui_text_baseline_right(u, col_clip, rb, FONT_MONO, g_pal.text_faint, "—");
        }
        if (i < 7)
            ui_line(u, c.x + PAD, yy + 24, right, yy + 24, g_pal.line_soft, 1);
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint, "VERDICT");
    ui_text_baseline(u, rx, r.y + 54, FONT_CRIT, m->clip ? g_pal.bad : g_pal.good,
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
    ui_text(u, x, y, FONT_VALUE, g_pal.text, "Tape deck (demo)");
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
            "tone −20 dBFS → selected out / in");

    ASRect c = center_card();
    mat_raised(u, c, RADIUS_CARD);
    title_block(u, c, "REPAIR PLAN — TAPE (SIMULATED)", "fixtures, not a DUT result");
    double tx = c.x + PAD + 8, ty = c.y + 56;
    double tw = c.w - PAD * 2 - 16;
    double right = c.x + c.w - PAD;
    double col_val = tx + tw * 0.46;
    double col_unit = tx + tw * 0.60;
    ui_text_baseline(u, tx, ty + 11, FONT_SMALL, g_pal.text_faint, "STEP");
    ui_text_baseline_right(u, col_val, ty + 11, FONT_SMALL, g_pal.text_faint,
                           "VALUE");
    ui_text_baseline(u, col_unit, ty + 11, FONT_SMALL, g_pal.text_faint,
                     "UNIT");
    ui_text_baseline_right(u, right, ty + 11, FONT_SMALL, g_pal.text_faint,
                           "RESULT");
    ui_line(u, c.x + PAD, ty + 20, right, ty + 20, g_pal.line_soft, 1);
    for (size_t i = 0; i < m->plan.size(); ++i) {
        const NativePlanStep &s = m->plan[i];
        double yy = ty + 34 + (double)i * 40.0;
        double rb = yy + 12;
        ui_text_baseline(u, tx, rb, FONT_MONO, g_pal.text, s.label.c_str());
        ui_text_baseline_right(u, col_val, rb, FONT_MONO, g_pal.text,
                               fmt(s.value, 4).c_str());
        ui_text_baseline(u, col_unit, rb, FONT_MONO, g_pal.text_faint,
                         s.unit.c_str());
        ui_text_baseline_right(u, right, rb, FONT_SMALL,
                               s.pass ? g_pal.good : g_pal.bad,
                               s.pass ? "PASS" : "FAIL");
        if (i + 1 < m->plan.size())
            ui_line(u, c.x + PAD, yy + 26, right, yy + 26, g_pal.line_soft, 1);
    }
    if (!m->plan_ran) {
        ui_text_baseline(u, tx, ty + 52, FONT_SMALL, g_pal.text_faint,
                         "Run the plan to fill in wow / flutter / azimuth / THD.");
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint,
                     "PLAN VERDICT");
    bool pass = m->plan_ran ? m->plan_pass : true;
    ui_text_baseline(u, rx, r.y + 54, FONT_CRIT, pass ? g_pal.good : g_pal.bad,
                     m->plan_ran ? (pass ? "DEMO PASS" : "DEMO FAIL") : "—");
    if (ui_button(u, 301, as_rect(rx, r.y + 96, rw, 32), "EXPORT PDF", BTN_NORMAL))
        act->export_report = true;
    ui_text(u, rx, r.y + 140, FONT_SMALL, g_pal.text_faint, "writes PDF + JSON + HTML");
    ui_text(u, rx, r.y + 158, FONT_SMALL, g_pal.warn,
            "synthetic fixtures — not a DUT result");
    ui_text(u, rx, r.y + 180, FONT_SMALL,
            m->loopback_ran ? (m->loopback_pass ? g_pal.good : g_pal.bad)
                            : g_pal.text_faint,
            m->loopback_ran
                ? (m->loopback_pass ? "connectivity OK" : "connectivity FAIL")
                : "connectivity: not run");
    if (m->loopback_ran) {
        label_value(u, rx, r.y + 204, rw, "LEVEL", fmt(m->loopback_level, 1) + " dBFS", g_pal.text);
        label_value(u, rx, r.y + 230, rw, "THD", fmt(m->loopback_thd, 3) + " %", g_pal.text);
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
    double tx = c.x + PAD + 8;
    double tw = c.w - PAD * 2 - 16;
    double right = c.x + c.w - PAD;
    double ty = c.y + 56;
    double col_kind = tx + tw * 0.50;
    double col_time = tx + tw * 0.78;
    ui_text_baseline(u, tx, ty + 11, FONT_SMALL, g_pal.text_faint, "FILE");
    ui_text_baseline(u, col_kind, ty + 11, FONT_SMALL, g_pal.text_faint, "KIND");
    ui_text_baseline_right(u, col_time, ty + 11, FONT_SMALL, g_pal.text_faint,
                           "TIME");
    ui_text_baseline_right(u, right, ty + 11, FONT_SMALL, g_pal.text_faint,
                           "RESULT");
    ui_line(u, c.x + PAD, ty + 20, right, ty + 20, g_pal.line_soft, 1);
    if (m->reports.empty()) {
        /* 空状态：发生了什么 / 为什么 / 下一步，三行以内（BERRY-UI-SPEC §6.6）。 */
        double bw = 56, bh = 56;
        double bx = c.x + (c.w - bw) * 0.5;
        double by = c.y + c.h * 0.42;
        ui_fill_round(u, as_rect(bx, by, bw, bh), 6, g_pal.card_alt);
        ui_stroke_round(u, as_rect(bx, by, bw, bh), 6, g_pal.line_soft, 1);
        ui_line(u, bx + 16, by + 28, bx + 40, by + 28, g_pal.text_faint, 1);
        ui_line(u, bx + 16, by + 34, bx + 32, by + 34, g_pal.text_faint, 1);
        const char *l1 = "No reports yet";
        const char *l2 = "Export one from the repair page or the button on the right.";
        const char *l3 = "Reports stay on this machine; nothing is uploaded.";
        double w1 = ui_text_width(u, FONT_LABEL, l1);
        double w2 = ui_text_width(u, FONT_SMALL, l2);
        double w3 = ui_text_width(u, FONT_SMALL, l3);
        ui_text_baseline(u, c.x + (c.w - w1) * 0.5, by + bh + 26, FONT_LABEL,
                         g_pal.text, l1);
        ui_text_baseline(u, c.x + (c.w - w2) * 0.5, by + bh + 46, FONT_SMALL,
                         g_pal.text_dim, l2);
        ui_text_baseline(u, c.x + (c.w - w3) * 0.5, by + bh + 64, FONT_SMALL,
                         g_pal.text_faint, l3);
    }
    for (size_t i = 0; i < m->reports.size(); ++i) {
        const NativeReport &rep = m->reports[i];
        double yy = ty + 36 + (double)i * 34.0;
        double rb = yy + 12;
        ui_text_ellipsis(u, as_rect(tx, yy, col_kind - tx - 12, 18), FONT_MONO,
                         g_pal.text, rep.name.c_str());
        ui_text_baseline(u, col_kind, rb, FONT_MONO, g_pal.text_dim,
                         rep.kind.c_str());
        ui_text_baseline_right(u, col_time, rb, FONT_MONO, g_pal.text_dim,
                               rep.time.c_str());
        ui_text_baseline_right(u, right, rb, FONT_SMALL,
                               rep.pass ? g_pal.good : g_pal.warn,
                               rep.pass ? "PASS" : "WARN");
        if (i + 1 < m->reports.size())
            ui_line(u, c.x + PAD, yy + 24, right, yy + 24, g_pal.line_soft, 1);
    }

    ASRect r = right_card();
    mat_raised(u, r, RADIUS_CARD);
    double rx = r.x + PAD, rw = r.w - PAD * 2;
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint, "EXPORT");
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
    ui_text_baseline(u, rx, r.y + 22, FONT_SMALL, g_pal.text_faint, "SAFETY");
    ui_text_baseline(u, rx, r.y + 44, FONT_SMALL, g_pal.text_dim,
                     "software cannot protect");
    ui_text_baseline(u, rx, r.y + 62, FONT_SMALL, g_pal.text_dim,
                     "the sound card.");
    ui_text_baseline(u, rx, r.y + 88, FONT_SMALL, g_pal.text_faint,
                     "use a protected front end");
    ui_text_baseline(u, rx, r.y + 106, FONT_SMALL, g_pal.text_faint,
                     "for unknown or high levels.");
}

void draw_footer(UI *u, NativeModel *m) {
    ui_line(u, 0, H - kFooterH, W, H - kFooterH, g_pal.line_soft, 1);
    double baseline = H - kFooterH + 21;
    char left[160];
    std::snprintf(left, sizeof(left), "%s · %.0f Hz · %d ch · %s", m->capture ? "INPUT" : "SYNTHETIC",
                  m->fs, m->channels, m->hold ? "HOLD" : "LIVE");
    ui_text_baseline(u, PAD, baseline, FONT_MONO, g_pal.text_faint, left);
    ui_text_baseline_right(u, W - PAD, baseline, FONT_MONO, g_pal.text_faint,
                           m->message.c_str());
    /* 品牌副行居中：用右对齐的逆运算不如直接量宽，保持真正的水平居中。 */
    const char *brand = "BERRYSTUDIO / " AS_VERSION;
    double bw = ui_text_width(u, FONT_MONO, brand);
    ui_text_baseline(u, (W - bw) * 0.5, baseline, FONT_MONO, g_pal.text_faint,
                     brand);
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
