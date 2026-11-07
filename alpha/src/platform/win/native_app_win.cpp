/* AudioScope by BerryStudio 0.9 — native Windows application.
 *
 * Win32 shell + GDI+ view. No WebView, no HTML, no local HTTP.
 * The file is deliberately self-contained: the small immediate-mode widget
 * layer below only exists to drive this one window, so the target needs just
 * audioscope_core, audioscope_platform, native_model.cpp, this file and
 * gdiplus/shell32.
 */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

#include "app/native_model.hpp"
#include "core/api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

namespace {

namespace gd = Gdiplus;

using audioscope::NativeChannel;
using audioscope::NativeEngine;
using audioscope::NativeModel;
using audioscope::NativePlanStep;
using audioscope::NativeReport;

/* --------------------------------------------------------------- layout */
constexpr float W = 1120.0f;
constexpr float H = 700.0f;
constexpr float HEADER_H = 64.0f;
constexpr float NAV_H = 48.0f;
constexpr float FOOTER_H = 32.0f;
constexpr float BODY_Y = HEADER_H + NAV_H;
constexpr float BODY_H = H - FOOTER_H - BODY_Y;
constexpr float PAD = 16.0f;
constexpr float LEFT_W = 232.0f;
constexpr float RIGHT_W = 216.0f;
constexpr float CENTER_X = PAD + LEFT_W + PAD;
constexpr float CENTER_W = W - CENTER_X - RIGHT_W - PAD * 2.0f;
constexpr float RIGHT_X = W - RIGHT_W - PAD;

struct Rect {
    float x;
    float y;
    float w;
    float h;
};

Rect rect(float x, float y, float w, float h) {
    Rect r = {x, y, w, h};
    return r;
}

Rect left_card() { return rect(PAD, BODY_Y + PAD, LEFT_W, BODY_H - PAD * 2.0f); }
Rect center_card() {
    return rect(CENTER_X, BODY_Y + PAD, CENTER_W, BODY_H - PAD * 2.0f);
}
Rect right_card() {
    return rect(RIGHT_X, BODY_Y + PAD, RIGHT_W, BODY_H - PAD * 2.0f);
}

bool hit_test(const Rect &r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

/* --------------------------------------------------------------- colours */
typedef DWORD Argb;

Argb with_alpha(Argb c, float a) {
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    const Argb al = static_cast<Argb>(a * 255.0f + 0.5f);
    return (c & 0x00ffffffu) | (al << 24);
}

Argb mix(Argb a, Argb b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    Argb out = 0;
    for (int sh = 0; sh <= 24; sh += 8) {
        const float ca = static_cast<float>((a >> sh) & 0xffu);
        const float cb = static_cast<float>((b >> sh) & 0xffu);
        const float v = ca + (cb - ca) * t;
        out |= static_cast<Argb>(v + 0.5f) << sh;
    }
    return out;
}

Argb brighten(Argb c, float amount) {
    return amount >= 0.0f ? mix(c, 0xffffffffu, amount)
                          : mix(c, 0x000000ffu, -amount);
}

struct Palette {
    Argb bg;
    Argb card;
    Argb card_alt;
    Argb line;
    Argb line_soft;
    Argb divider;
    Argb text;
    Argb text_dim;
    Argb text_faint;
    Argb accent;
    Argb accent_soft;
    Argb good;
    Argb info;
    Argb warn;
    Argb bad;
    Argb chart_bg;
    Argb chart_grid;
    Argb chart_axis;
    Argb curve_main;
    Argb curve_hold;
    Argb curve_fill;
};

const Palette PALETTE_DARK = {
    0xff171a1b, /* bg */
    0xff202425, /* card */
    0xff2a2f30, /* card_alt */
    0xff3b4242, /* line */
    0xff303637, /* line_soft */
    0xff303637, /* divider */
    0xff969c98, /* text */
    0xff909994, /* text_dim */
    0xff858f89, /* text_faint */
    0xff819e90, /* accent */
    0xff30463e, /* accent_soft */
    0xff819e90, /* good */
    0xff819e90, /* info */
    0xffb19870, /* warn */
    0xffc08278, /* bad */
    0xff202425, /* chart_bg */
    0xff303637, /* chart_grid */
    0xff858f89, /* chart_axis */
    0xffd6dad6, /* curve_main */
    0xff858f89, /* curve_hold */
    0xff819e90, /* curve_fill */
};

const Palette PALETTE_LIGHT = {
    0xffe5e6df, /* bg */
    0xffdce0d9, /* card */
    0xffe9ece5, /* card_alt */
    0xffc4cdc5, /* line */
    0xffd0d7cf, /* line_soft */
    0xffd0d7cf, /* divider */
    0xff26352e, /* text */
    0xff53665b, /* text_dim */
    0xff62776a, /* text_faint */
    0xff356d56, /* accent */
    0xffd6e5d9, /* accent_soft */
    0xff14835a, /* good */
    0xff356d56, /* info */
    0xffb46a10, /* warn */
    0xffc9302a, /* bad */
    0xff283133, /* chart_bg */
    0xff455153, /* chart_grid */
    0xffa7b3ae, /* chart_axis */
    0xffd3d7d3, /* curve_main */
    0xff9aa6a2, /* curve_hold */
    0xff356d56, /* curve_fill */
};

Palette g_pal = PALETTE_DARK;
bool g_light = false;

Argb plot_bg() { return g_light ? g_pal.chart_bg : g_pal.bg; }

/* --------------------------------------------------------------- app state */
NativeEngine g_engine;
NativeModel g_model;

ULONG_PTR g_gdiplus_token = 0;
gd::Bitmap *g_backbuffer = nullptr;
int g_backbuffer_w = 0;
int g_backbuffer_h = 0;

float g_scale = 1.0f;
float g_origin_x = 0.0f;
float g_origin_y = 0.0f;

float g_mouse_x = -10000.0f;
float g_mouse_y = -10000.0f;
bool g_mouse_down = false;
bool g_tracking_mouse = false;
int g_pressed_id = -1;

struct HitRegion {
    int id;
    Rect r;
};

std::vector<HitRegion> g_hits;

/* --------------------------------------------------------------- fonts */
enum FontKind {
    FONT_LABEL = 0,
    FONT_VALUE,
    FONT_CRIT,
    FONT_MONO,
    FONT_SMALL,
    FONT_BRAND,
    FONT_KIND_COUNT
};

gd::Font *g_fonts[FONT_KIND_COUNT] = {};
gd::StringFormat *g_format_left = nullptr;
gd::StringFormat *g_format_right = nullptr;
gd::StringFormat *g_format_center = nullptr;

enum TextAlign { TEXT_LEFT = 0, TEXT_RIGHT = 1, TEXT_CENTER = 2 };

gd::Font *font_at(FontKind kind) {
    const int i = std::clamp(static_cast<int>(kind), 0, FONT_KIND_COUNT - 1);
    return g_fonts[i];
}

std::wstring widen(const std::string &s) {
    if (s.empty()) return std::wstring();
    const int len = static_cast<int>(s.size());
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), len,
                                nullptr, 0);
    if (n > 0) {
        std::wstring out(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), len,
                            &out[0], n);
        return out;
    }
    n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), len, nullptr, 0);
    if (n > 0) {
        std::wstring out(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), len, &out[0], n);
        return out;
    }
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
        out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(s[i])));
    return out;
}

const std::wstring &app_version() {
    static const std::wstring v = widen(AS_VERSION);
    return v;
}

const std::wstring &app_brand() {
    static const std::wstring v = widen(AS_BRAND);
    return v;
}

std::wstring fmt(double value, int decimals) {
    wchar_t buf[64];
    std::swprintf(buf, 64, L"%.*f", decimals, value);
    return std::wstring(buf);
}

std::wstring fmt_signed(double value, int decimals) {
    wchar_t buf[64];
    std::swprintf(buf, 64, L"%+.*f", decimals, value);
    return std::wstring(buf);
}

gd::Font *make_font(const wchar_t *const *names, int count, float px) {
    for (int i = 0; i < count; ++i) {
        gd::Font *f = new gd::Font(names[i], px, gd::FontStyleRegular,
                                   gd::UnitPixel);
        if (f->GetLastStatus() == gd::Ok) return f;
        delete f;
    }
    return new gd::Font(L"Arial", px, gd::FontStyleRegular, gd::UnitPixel);
}

void init_fonts() {
    static const wchar_t *const ui_names[] = {
        L"Microsoft YaHei UI", L"Microsoft YaHei", L"SimSun", L"Segoe UI",
        L"Arial"};
    static const wchar_t *const mono_names[] = {
        L"Consolas", L"Lucida Console", L"Courier New", L"Arial"};

    g_fonts[FONT_LABEL] = make_font(ui_names, 5, 14.0f);
    g_fonts[FONT_VALUE] = make_font(mono_names, 4, 16.0f);
    g_fonts[FONT_CRIT] = make_font(mono_names, 4, 26.0f);
    g_fonts[FONT_MONO] = make_font(mono_names, 4, 12.0f);
    g_fonts[FONT_SMALL] = make_font(ui_names, 5, 11.0f);
    g_fonts[FONT_BRAND] = make_font(ui_names, 5, 16.0f);
}

void destroy_fonts() {
    for (int i = 0; i < FONT_KIND_COUNT; ++i) {
        delete g_fonts[i];
        g_fonts[i] = nullptr;
    }
}

gd::StringFormat *make_format(TextAlign align) {
    gd::StringFormat *f = new gd::StringFormat(gd::StringFormatFlagsNoWrap);
    f->SetAlignment(align == TEXT_RIGHT
                        ? gd::StringAlignmentFar
                        : (align == TEXT_CENTER ? gd::StringAlignmentCenter
                                                : gd::StringAlignmentNear));
    f->SetLineAlignment(gd::StringAlignmentCenter);
    f->SetTrimming(gd::StringTrimmingEllipsisCharacter);
    f->SetHotkeyPrefix(gd::HotkeyPrefixNone);
    return f;
}

void init_formats() {
    g_format_left = make_format(TEXT_LEFT);
    g_format_right = make_format(TEXT_RIGHT);
    g_format_center = make_format(TEXT_CENTER);
}

void destroy_formats() {
    delete g_format_left;
    delete g_format_right;
    delete g_format_center;
    g_format_left = nullptr;
    g_format_right = nullptr;
    g_format_center = nullptr;
}

/* --------------------------------------------------------------- drawing */
void add_hit(int id, const Rect &r) {
    HitRegion h;
    h.id = id;
    h.r = r;
    g_hits.push_back(h);
}

void fill_rect(gd::Graphics &g, float x, float y, float w, float h, Argb c) {
    if (w <= 0.0f || h <= 0.0f) return;
    gd::SolidBrush b{static_cast<gd::Color>(c)};
    g.FillRectangle(&b, x, y, w, h);
}

void add_round_rect(gd::GraphicsPath &p, const Rect &r, float radius) {
    float rad = radius;
    const float limit = std::min(r.w, r.h) * 0.5f;
    if (rad > limit) rad = limit;
    if (rad < 0.5f) {
        p.AddRectangle(gd::RectF(r.x, r.y, r.w, r.h));
        return;
    }
    const float d = rad * 2.0f;
    p.AddArc(r.x, r.y, d, d, 180.0f, 90.0f);
    p.AddArc(r.x + r.w - d, r.y, d, d, 270.0f, 90.0f);
    p.AddArc(r.x + r.w - d, r.y + r.h - d, d, d, 0.0f, 90.0f);
    p.AddArc(r.x, r.y + r.h - d, d, d, 90.0f, 90.0f);
    p.CloseFigure();
}

void fill_round(gd::Graphics &g, const Rect &r, float radius, Argb c) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    gd::GraphicsPath p;
    add_round_rect(p, r, radius);
    gd::SolidBrush b{static_cast<gd::Color>(c)};
    g.FillPath(&b, &p);
}

void stroke_round(gd::Graphics &g, const Rect &r, float radius, Argb c,
                  float width) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    gd::GraphicsPath p;
    add_round_rect(p, r, radius);
    gd::Pen pen(gd::Color(c), width);
    g.DrawPath(&pen, &p);
}

void line(gd::Graphics &g, float x1, float y1, float x2, float y2, Argb c,
          float width) {
    gd::Pen pen(gd::Color(c), width);
    g.DrawLine(&pen, x1, y1, x2, y2);
}

void dot(gd::Graphics &g, float cx, float cy, float radius, Argb c) {
    gd::SolidBrush b{static_cast<gd::Color>(c)};
    g.FillEllipse(&b, cx - radius, cy - radius, radius * 2.0f,
                  radius * 2.0f);
}

float text_width(gd::Graphics &g, FontKind kind, const std::wstring &value) {
    if (value.empty()) return 0.0f;
    /* Measure in world units, independent of the paint-time scale. */
    const gd::GraphicsState state = g.Save();
    g.ResetTransform();
    gd::RectF bounds;
    g.MeasureString(value.c_str(), static_cast<INT>(value.size()), font_at(kind),
                    gd::PointF(0.0f, 0.0f), &bounds);
    g.Restore(state);
    return bounds.Width;
}

void text(gd::Graphics &g, float x, float y, FontKind kind, Argb c,
          const std::wstring &value) {
    if (value.empty()) return;
    gd::SolidBrush b{static_cast<gd::Color>(c)};
    g.DrawString(value.c_str(), static_cast<INT>(value.size()), font_at(kind),
                 gd::PointF(x, y), &b);
}

void text_rect(gd::Graphics &g, const Rect &r, FontKind kind, Argb c,
               const std::wstring &value, TextAlign align) {
    if (value.empty() || r.w <= 0.0f || r.h <= 0.0f) return;
    gd::SolidBrush b{static_cast<gd::Color>(c)};
    gd::StringFormat *format = align == TEXT_RIGHT
                                   ? g_format_right
                                   : (align == TEXT_CENTER ? g_format_center
                                                           : g_format_left);
    g.DrawString(value.c_str(), static_cast<INT>(value.size()), font_at(kind),
                 gd::RectF(r.x, r.y, r.w, r.h), format, &b);
}

/* --------------------------------------------------------------- materials */
void card(gd::Graphics &g, const Rect &r) {
    fill_round(g, r, 6.0f, g_pal.card);
    line(g, r.x + 6.0f, r.y + 0.5f, r.x + r.w - 6.0f, r.y + 0.5f,
         with_alpha(0xffffffffu, g_light ? 0.55f : 0.10f), 1.0f);
    line(g, r.x + 6.0f, r.y + r.h - 0.5f, r.x + r.w - 6.0f,
         r.y + r.h - 0.5f, with_alpha(0x000000ffu, 0.22f), 1.0f);
    stroke_round(g, r, 6.0f, g_pal.line_soft, 1.0f);
}

void recessed(gd::Graphics &g, const Rect &r) {
    fill_round(g, r, 4.0f, plot_bg());
    line(g, r.x + 4.0f, r.y + 0.5f, r.x + r.w - 4.0f, r.y + 0.5f,
         with_alpha(0x000000ffu, g_light ? 0.10f : 0.34f), 1.0f);
    line(g, r.x + 4.0f, r.y + r.h - 0.5f, r.x + r.w - 4.0f,
         r.y + r.h - 0.5f, with_alpha(0xffffffffu, g_light ? 0.65f : 0.08f),
         1.0f);
    stroke_round(g, r, 4.0f, g_pal.line, 1.0f);
}

void group_heading(gd::Graphics &g, float x, float y, float w,
                   const std::wstring &title) {
    text_rect(g, rect(x, y - 1.0f, w, 20.0f), FONT_LABEL, g_pal.text_dim,
              title, TEXT_LEFT);
    line(g, x, y + 20.0f, x + w, y + 20.0f, g_pal.line_soft, 1.0f);
}

float group(gd::Graphics &g, float x, float y, float w,
            const std::wstring &title) {
    group_heading(g, x, y, w, title);
    return y + 28.0f;
}

void title_block(gd::Graphics &g, const Rect &r, const std::wstring &title,
                 const std::wstring &sub) {
    text(g, r.x + PAD, r.y + 12.0f, FONT_SMALL, g_pal.text_faint, title);
    text_rect(g, rect(r.x + PAD, r.y + 10.0f, r.w - PAD * 2.0f, 16.0f),
              FONT_SMALL, g_pal.text_faint, sub, TEXT_RIGHT);
    line(g, r.x + PAD, r.y + 32.0f, r.x + r.w - PAD, r.y + 32.0f,
         g_pal.line_soft, 1.0f);
}

void label_value(gd::Graphics &g, float x, float y, float w,
                 const std::wstring &label, const std::wstring &value,
                 Argb color) {
    text_rect(g, rect(x, y, w, 18.0f), FONT_SMALL, g_pal.text_faint, label,
              TEXT_LEFT);
    text_rect(g, rect(x, y - 2.0f, w, 20.0f), FONT_VALUE, color, value,
              TEXT_RIGHT);
}

/* --------------------------------------------------------------- widgets */
enum ButtonStyle { BTN_NORMAL = 0, BTN_PRIMARY, BTN_DANGER, BTN_GHOST };

void button(gd::Graphics &g, int id, const Rect &r, const std::wstring &label,
            ButtonStyle style) {
    add_hit(id, r);
    const bool hover = hit_test(r, g_mouse_x, g_mouse_y);
    const bool pressed = g_mouse_down && g_pressed_id == id && hover;

    Argb bg = g_pal.card_alt;
    Argb fg = g_pal.text;
    Argb border = g_pal.line;
    bool has_bg = true;

    if (style == BTN_PRIMARY) {
        bg = g_pal.accent_soft;
        fg = g_pal.accent;
        border = with_alpha(g_pal.accent, 0.75f);
    } else if (style == BTN_DANGER) {
        bg = g_pal.card_alt;
        fg = g_pal.bad;
        border = with_alpha(g_pal.bad, 0.50f);
    } else if (style == BTN_GHOST) {
        has_bg = false;
        fg = g_pal.text_dim;
    }

    if (hover && has_bg)
        bg = brighten(bg, g_light ? 0.045f : 0.085f);
    if (hover && !has_bg)
        fill_round(g, r, 5.0f, with_alpha(0xffffffffu, 0.05f));
    if (pressed && has_bg) bg = brighten(bg, -0.16f);

    if (has_bg) {
        fill_round(g, r, 5.0f, bg);
        stroke_round(g, r, 5.0f, border, 1.0f);
    }

    Rect tr = r;
    if (pressed) tr.y += 1.0f;
    text_rect(g, tr, FONT_LABEL, fg, label, TEXT_CENTER);
}

void toggle(gd::Graphics &g, int id, const Rect &r,
            const std::wstring &label, bool on) {
    add_hit(id, r);
    const bool hover = hit_test(r, g_mouse_x, g_mouse_y);
    const bool pressed = g_mouse_down && g_pressed_id == id && hover;

    Argb bg = g_pal.card_alt;
    if (on) bg = mix(g_pal.card_alt, g_pal.accent_soft, 0.55f);
    if (hover) bg = brighten(bg, g_light ? 0.04f : 0.07f);
    if (pressed) bg = brighten(bg, -0.12f);

    fill_round(g, r, 5.0f, bg);
    stroke_round(g, r, 5.0f,
                 on ? with_alpha(g_pal.accent, 0.55f) : g_pal.line, 1.0f);
    if (on) fill_round(g, rect(r.x + 1.0f, r.y + 6.0f, 2.0f, r.h - 12.0f),
                       1.0f, g_pal.accent);

    Rect tr = r;
    if (on) {
        tr.x += 8.0f;
        tr.w -= 8.0f;
    }
    text_rect(g, tr, FONT_LABEL, on ? g_pal.accent : g_pal.text_faint, label,
              TEXT_CENTER);
}

/* --------------------------------------------------------------- charts */
void plot_grid(gd::Graphics &g, const Rect &p, int cols, int rows) {
    for (int i = 0; i <= cols; ++i) {
        const float x = p.x + p.w * static_cast<float>(i) /
                                  static_cast<float>(cols);
        line(g, x, p.y, x, p.y + p.h, g_pal.chart_grid, 1.0f);
    }
    for (int i = 0; i <= rows; ++i) {
        const float y = p.y + p.h * static_cast<float>(i) /
                                  static_cast<float>(rows);
        line(g, p.x, y, p.x + p.w, y, g_pal.chart_grid, 1.0f);
    }
}

float db_to_y(double db, const Rect &p) {
    float t = static_cast<float>((db + 100.0) / 100.0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return p.y + p.h - t * p.h;
}

/* --------------------------------------------------------------- header */
void draw_header(gd::Graphics &g) {
    fill_rect(g, 0.0f, 0.0f, W, HEADER_H, g_pal.bg);

    dot(g, 30.0f, 26.0f, 9.0f, g_pal.accent);
    dot(g, 30.0f, 26.0f, 3.0f, g_pal.bg);
    text(g, 48.0f, 14.0f, FONT_BRAND, g_pal.text, L"AUDIOSCOPE");
    text(g, 48.0f, 36.0f, FONT_SMALL, g_pal.text_faint,
         std::wstring(L"BERRYSTUDIO / ") + app_version());

    text_rect(g, rect(W - 420.0f, 14.0f, 250.0f, 16.0f), FONT_SMALL,
              g_model.capture ? g_pal.warn : g_pal.good,
              g_model.capture ? L"CAPTURE · INPUT" : L"SYNTHETIC SOURCE",
              TEXT_RIGHT);
    text_rect(g, rect(W - 420.0f, 34.0f, 250.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, widen(g_model.device), TEXT_RIGHT);

    button(g, 900, rect(W - 150.0f, 16.0f, 62.0f, 30.0f),
           g_model.light ? L"LIGHT" : L"DARK", BTN_GHOST);
    button(g, 901, rect(W - 80.0f, 16.0f, 62.0f, 30.0f), L"ABOUT",
           BTN_GHOST);
}

void draw_nav(gd::Graphics &g) {
    static const wchar_t *const labels[audioscope::PAGE_COUNT] = {
        L"波形", L"频谱", L"相位", L"测量", L"维修", L"报告", L"校准"};

    const float y = HEADER_H;
    float x = PAD;
    for (int i = 0; i < audioscope::PAGE_COUNT; ++i) {
        const std::wstring label(labels[i]);
        const float w = text_width(g, FONT_LABEL, label) + 32.0f;
        const Rect r = rect(x, y, w, NAV_H);
        const bool selected = g_model.page == i;
        const bool hover = hit_test(r, g_mouse_x, g_mouse_y);

        if (hover && !selected)
            fill_round(g, rect(x + 4.0f, y + 10.0f, w - 8.0f, NAV_H - 20.0f),
                       4.0f, with_alpha(0xffffffffu, 0.04f));
        text_rect(g, r, FONT_LABEL,
                  selected ? g_pal.accent
                           : (hover ? g_pal.text : g_pal.text_faint),
                  label, TEXT_CENTER);
        add_hit(100 + i, r);
        if (selected) {
            const float ix = x + w * 0.15f;
            const float iw = w * 0.70f;
            fill_round(g, rect(ix, y + NAV_H - 2.0f, iw, 2.0f), 1.0f,
                       g_pal.accent);
        }
        x += w + 4.0f;
    }
    line(g, PAD, y + NAV_H - 1.0f, W - PAD, y + NAV_H - 1.0f,
         g_pal.line_soft, 1.0f);
}

/* --------------------------------------------------------------- pages */
void draw_scope(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"SOURCE");

    text_rect(g, rect(x, y, w, 18.0f), FONT_SMALL, g_pal.text_faint, L"INPUT",
              TEXT_LEFT);
    text_rect(g, rect(x, y - 2.0f, w, 20.0f), FONT_VALUE, g_pal.text,
              m.capture ? L"CAPTURE" : L"SYNTHETIC", TEXT_RIGHT);
    label_value(g, x, y + 26.0f, w, L"DEVICE", widen(m.device), g_pal.text);
    label_value(g, x, y + 50.0f, w, L"RATE", fmt(m.fs, 0) + L" Hz",
                g_pal.text);
    label_value(g, x, y + 74.0f, w, L"MODE", L"Inst", g_pal.text);

    button(g, 200, rect(x, y + 100.0f, w, 30.0f),
           m.capture ? L"STOP CAPTURE" : L"CAPTURE INPUT",
           m.capture ? BTN_DANGER : BTN_NORMAL);
    toggle(g, 201, rect(x, y + 138.0f, w, 30.0f), L"HOLD  (freeze)",
           m.hold);

    y = group(g, x, y + 190.0f, w, L"CHANNELS");
    text_rect(g, rect(x, y, w, 18.0f), FONT_SMALL, g_pal.text_dim,
              L"CH1  ·  AC  ·  ±1.0 V", TEXT_LEFT);
    text_rect(g, rect(x, y + 22.0f, w, 18.0f), FONT_SMALL, g_pal.text_faint,
              L"CH2  ·  AC  ·  ±1.0 V", TEXT_LEFT);

    y = group(g, x, y + 58.0f, w, L"TIMEBASE");
    text(g, x, y, FONT_VALUE, g_pal.text, L"1.50 ms/div");
    text(g, x, y + 26.0f, FONT_SMALL, g_pal.text_faint,
         L"trigger: CH1 rising · 0.00 V");

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"TIME DOMAIN", L"CH1 · 48 kHz · 15 ms window");

    Rect plot = rect(c.x + PAD + 24.0f, c.y + 48.0f, c.w - PAD * 2.0f - 40.0f,
                     c.h - 96.0f);
    recessed(g, plot);
    plot_grid(g, plot, 10, 8);
    const float mid = plot.y + plot.h * 0.5f;
    line(g, plot.x, mid, plot.x + plot.w, mid, g_pal.chart_axis, 1.0f);

    if (m.wave.size() > 1) {
        const int n = static_cast<int>(m.wave.size());
        std::vector<gd::PointF> pts;
        pts.reserve(m.wave.size());
        for (int i = 0; i < n; ++i) {
            const float tx = plot.x + plot.w * static_cast<float>(i) /
                                         static_cast<float>(n - 1);
            const float v = std::clamp(static_cast<float>(m.wave[i]), -1.0f,
                                       1.0f);
            const float ty = mid - v * plot.h * 0.44f;
            pts.push_back(gd::PointF(tx, ty));
        }
        if (pts.size() >= 2) {
            gd::Pen pen(gd::Color(g_pal.curve_main), 1.4f);
            g.DrawLines(&pen, pts.data(), static_cast<INT>(pts.size()));
        }
    }

    gd::Pen trigger(gd::Color(g_pal.warn), 1.0f);
    trigger.SetDashStyle(gd::DashStyleDash);
    g.DrawLine(&trigger, plot.x + plot.w * 0.5f, plot.y,
               plot.x + plot.w * 0.5f, plot.y + plot.h);

    text_rect(g, rect(plot.x, plot.y + plot.h + 4.0f, plot.w, 14.0f),
              FONT_SMALL, g_pal.text_faint,
              L"15.0 ms  ·  10 div  ·  −1.0 … +1.0 FS", TEXT_RIGHT);

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    const float rw = r.w - PAD * 2.0f;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint,
         L"INPUT PEAK · CH1");
    text(g, rx, r.y + 30.0f, FONT_CRIT, g_pal.accent,
         fmt(as_linear_to_db(m.stats.peak), 1) + L" dBFS");

    float yy = r.y + 78.0f;
    label_value(g, rx, yy, rw, L"RMS",
                fmt(as_linear_to_db(m.stats.rms), 1) + L" dBFS",
                g_pal.text);
    label_value(g, rx, yy + 26.0f, rw, L"FREQUENCY",
                fmt(m.stats.freq, 1) + L" Hz", g_pal.text);
    label_value(
        g, rx, yy + 52.0f, rw, L"CREST",
        fmt(20.0 * std::log10(m.stats.peak / std::max(m.stats.rms, 1e-9)),
            1) +
            L" dB",
        g_pal.text);
    label_value(g, rx, yy + 78.0f, rw, L"Vpp", fmt(m.stats.vpp, 3) + L" FS",
                g_pal.text);
    label_value(g, rx, yy + 104.0f, rw, L"CLIP",
                m.clip ? L"LATCHED" : L"0",
                m.clip ? g_pal.bad : g_pal.text);

    line(g, rx, yy + 136.0f, rx + rw, yy + 136.0f, g_pal.line_soft, 1.0f);
    text(g, rx, yy + 150.0f, FONT_SMALL, g_pal.text_faint,
         m.cal_fs_vrms > 0.0 ? L"voltage: calibrated"
                             : L"voltage: uncalibrated");
    text(g, rx, yy + 170.0f, FONT_SMALL,
         m.cal_fs_vrms > 0.0 ? g_pal.good : g_pal.warn,
         m.cal_fs_vrms > 0.0 ? L"measured" : L"dBFS only");
}

void draw_spectrum(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"FFT");
    label_value(g, x, y, w, L"SIZE", L"4096", g_pal.text);
    label_value(g, x, y + 26.0f, w, L"WINDOW", L"Hann", g_pal.text);
    label_value(g, x, y + 52.0f, w, L"RATE", fmt(m.fs, 0) + L" Hz",
                g_pal.text);

    y = group(g, x, y + 92.0f, w, L"SOURCE");
    text(g, x, y, FONT_SMALL, g_pal.text_dim,
         m.capture ? L"capture · CH1" : L"synthetic · CH1");
    text(g, x, y + 24.0f, FONT_SMALL, g_pal.text_faint,
         L"peak-relative, 100 dB floor");

    y = group(g, x, y + 62.0f, w, L"HARMONICS");
    text(g, x, y, FONT_SMALL, g_pal.text_dim, L"H2  −58.1 dB");
    text(g, x, y + 22.0f, FONT_SMALL, g_pal.text_faint, L"H3  −62.4 dB");

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"SPECTRUM", L"Hann · 4096 · 20 Hz – 20 kHz");

    Rect p = rect(c.x + PAD + 32.0f, c.y + 48.0f,
                  c.w - PAD * 2.0f - 48.0f, c.h - 96.0f);
    recessed(g, p);
    for (int i = 0; i <= 5; ++i) {
        const float yy = p.y + p.h * static_cast<float>(i) / 5.0f;
        line(g, p.x, yy, p.x + p.w, yy, g_pal.chart_grid, 1.0f);
    }
    for (int i = 0; i <= 4; ++i) {
        const float xx = p.x + p.w * static_cast<float>(i) / 4.0f;
        line(g, xx, p.y, xx, p.y + p.h, g_pal.chart_grid, 1.0f);
    }

    if (m.spec_db.size() > 1) {
        const int n = static_cast<int>(m.spec_db.size());
        std::vector<gd::PointF> pts;
        pts.reserve(m.spec_db.size());
        for (int i = 0; i < n; ++i) {
            const float tx = p.x + p.w * static_cast<float>(i) /
                                        static_cast<float>(n - 1);
            const float ty = db_to_y(m.spec_db[i], p);
            pts.push_back(gd::PointF(tx, ty));
        }
        if (pts.size() >= 2) {
            gd::Pen pen(gd::Color(g_pal.curve_main), 1.3f);
            g.DrawLines(&pen, pts.data(), static_cast<INT>(pts.size()));
        }
    }
    text_rect(g, rect(p.x, p.y + p.h + 4.0f, p.w, 14.0f), FONT_SMALL,
              g_pal.text_faint,
              L"20 Hz                       1 kHz                       20 kHz",
              TEXT_RIGHT);

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    const float rw = r.w - PAD * 2.0f;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint,
         L"TOTAL HARMONIC DISTORTION");
    text(g, rx, r.y + 30.0f, FONT_CRIT, g_pal.accent,
         fmt(m.tone.thd_pct, 3) + L" %");

    float yy = r.y + 78.0f;
    label_value(g, rx, yy, rw, L"THD+N", fmt(m.tone.thdn_pct, 3) + L" %",
                g_pal.text);
    label_value(g, rx, yy + 26.0f, rw, L"SNR", fmt(m.tone.snr_db, 1) + L" dB",
                g_pal.text);
    label_value(g, rx, yy + 52.0f, rw, L"SINAD",
                fmt(m.tone.sinad_db, 1) + L" dB", g_pal.text);
    label_value(g, rx, yy + 78.0f, rw, L"SFDR",
                fmt(m.tone.sfdr_db, 1) + L" dB", g_pal.text);
    label_value(g, rx, yy + 104.0f, rw, L"ENOB",
                fmt(m.tone.enob, 2) + L" bit", g_pal.text);

    line(g, rx, yy + 136.0f, rx + rw, yy + 136.0f, g_pal.line_soft, 1.0f);
    text(g, rx, yy + 150.0f, FONT_SMALL, g_pal.text_faint,
         L"single-tone FFT; not a full IMD suite");
}

void draw_xy(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"PAIR");
    text(g, x, y, FONT_BRAND, g_pal.text, L"CH1 → X");
    text(g, x, y + 24.0f, FONT_BRAND, g_pal.text, L"CH2 → Y");

    y = group(g, x, y + 60.0f, w, L"DISPLAY");
    label_value(g, x, y, w, L"PERSISTENCE", L"2 s", g_pal.text);
    label_value(g, x, y + 26.0f, w, L"GAIN", fmt(m.gain_db, 2) + L" dB",
                g_pal.text);

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"PHASE / XY", L"Lissajous · CH1 vs CH2");

    const float size = std::min(c.w - PAD * 4.0f, c.h - 96.0f);
    Rect p = rect(c.x + (c.w - size) * 0.5f, c.y + 48.0f, size, size);
    recessed(g, p);
    for (int i = 1; i < 4; ++i) {
        const float xx = p.x + p.w * static_cast<float>(i) / 4.0f;
        const float yy = p.y + p.h * static_cast<float>(i) / 4.0f;
        line(g, xx, p.y, xx, p.y + p.h, g_pal.chart_grid, 1.0f);
        line(g, p.x, yy, p.x + p.w, yy, g_pal.chart_grid, 1.0f);
    }
    line(g, p.x, p.y + p.h * 0.5f, p.x + p.w, p.y + p.h * 0.5f,
         g_pal.chart_axis, 1.0f);
    line(g, p.x + p.w * 0.5f, p.y, p.x + p.w * 0.5f, p.y + p.h,
         g_pal.chart_axis, 1.0f);

    if (m.xy_x.size() == m.xy_y.size()) {
        g.SetClip(gd::RectF(p.x, p.y, p.w, p.h), gd::CombineModeReplace);
        gd::SolidBrush b{static_cast<gd::Color>(with_alpha(g_pal.accent, 0.45f))};
        for (size_t i = 0; i < m.xy_x.size(); ++i) {
            const float tx = p.x + p.w * 0.5f +
                             static_cast<float>(m.xy_x[i]) * p.w * 0.45f;
            const float ty = p.y + p.h * 0.5f -
                             static_cast<float>(m.xy_y[i]) * p.h * 0.45f;
            g.FillEllipse(&b, tx - 1.3f, ty - 1.3f, 2.6f, 2.6f);
        }
        g.ResetClip();
    }

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    const float rw = r.w - PAD * 2.0f;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint, L"PHASE");
    text(g, rx, r.y + 30.0f, FONT_CRIT, g_pal.accent,
         fmt(m.phase_deg, 1) + L"°");

    float yy = r.y + 78.0f;
    label_value(g, rx, yy, rw, L"GAIN", fmt_signed(m.gain_db, 2) + L" dB",
                g_pal.text);
    label_value(g, rx, yy + 26.0f, rw, L"FREQUENCY",
                fmt(m.stats.freq, 1) + L" Hz", g_pal.text);
    label_value(g, rx, yy + 52.0f, rw, L"POLARITY", L"normal", g_pal.good);

    line(g, rx, yy + 84.0f, rx + rw, yy + 84.0f, g_pal.line_soft, 1.0f);
    text(g, rx, yy + 98.0f, FONT_SMALL, g_pal.text_faint,
         L"same device: channels share a clock");
}

void draw_measure(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"RUN");
    label_value(g, x, y, w, L"FRAMES", L"720", g_pal.text);
    label_value(g, x, y + 26.0f, w, L"WINDOW", L"15 ms", g_pal.text);
    label_value(g, x, y + 52.0f, w, L"REPEAT", L"1×", g_pal.text);

    y = group(g, x, y + 92.0f, w, L"LIMITS");
    text(g, x, y, FONT_SMALL, g_pal.text_dim, L"clip ≤ 0");
    text(g, x, y + 22.0f, FONT_SMALL, g_pal.text_faint, L"20 Hz – 20 kHz");
    text(g, x, y + 44.0f, FONT_SMALL, g_pal.text_faint,
         L"uncalibrated → dBFS only");

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"CHANNEL MEASUREMENTS",
                L"8 channels · current frame");

    const float ty = c.y + 52.0f;
    const float col_ch = c.x + 24.0f;
    const float col_peak = c.x + 190.0f;
    const float col_rms = c.x + 290.0f;
    const float col_freq = c.x + 390.0f;
    const float col_vpp = c.x + 470.0f;
    const float col_clip = c.x + 524.0f;
    const float col_pass = c.x + c.w - 24.0f;

    text(g, col_ch, ty, FONT_SMALL, g_pal.text_faint, L"CH");
    text_rect(g, rect(col_peak - 110.0f, ty, 110.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"PEAK dBFS", TEXT_RIGHT);
    text_rect(g, rect(col_rms - 100.0f, ty, 100.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"RMS dBFS", TEXT_RIGHT);
    text_rect(g, rect(col_freq - 100.0f, ty, 100.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"FREQ Hz", TEXT_RIGHT);
    text_rect(g, rect(col_vpp - 80.0f, ty, 80.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"Vpp FS", TEXT_RIGHT);
    text_rect(g, rect(col_clip - 54.0f, ty, 54.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"CLIP", TEXT_RIGHT);
    line(g, c.x + PAD, ty + 20.0f, c.x + c.w - PAD, ty + 20.0f,
         g_pal.line_soft, 1.0f);

    int active_count = 0;
    for (int i = 0; i < 8; ++i) {
        const float yy = ty + 32.0f + static_cast<float>(i) * 34.0f;
        const NativeChannel &ch = m.ch[i];
        if (ch.active) ++active_count;

        text(g, col_ch, yy, FONT_MONO,
             ch.active ? g_pal.text : g_pal.text_faint,
             std::wstring(L"CH") + std::to_wstring(i + 1));

        if (ch.active) {
            text_rect(g, rect(col_peak - 100.0f, yy, 100.0f, 18.0f),
                      FONT_MONO, g_pal.text, fmt(ch.peak_db, 1), TEXT_RIGHT);
            text_rect(g, rect(col_rms - 100.0f, yy, 100.0f, 18.0f),
                      FONT_MONO, g_pal.text, fmt(ch.rms_db, 1), TEXT_RIGHT);
            text_rect(g, rect(col_freq - 100.0f, yy, 100.0f, 18.0f),
                      FONT_MONO, g_pal.text, fmt(ch.freq, 1), TEXT_RIGHT);
            text_rect(g, rect(col_vpp - 80.0f, yy, 80.0f, 18.0f), FONT_MONO,
                      g_pal.text, fmt(ch.vpp, 3), TEXT_RIGHT);
            text_rect(g, rect(col_clip - 54.0f, yy, 54.0f, 18.0f), FONT_MONO,
                      g_pal.text, std::to_wstring(ch.clip), TEXT_RIGHT);
            text_rect(g, rect(col_pass - 60.0f, yy - 2.0f, 60.0f, 18.0f),
                      FONT_SMALL, g_pal.good, L"PASS", TEXT_RIGHT);
        } else {
            text_rect(g, rect(col_peak - 100.0f, yy, 100.0f, 18.0f),
                      FONT_MONO, g_pal.text_faint, L"—", TEXT_RIGHT);
            text_rect(g, rect(col_rms - 100.0f, yy, 100.0f, 18.0f),
                      FONT_MONO, g_pal.text_faint, L"—", TEXT_RIGHT);
            text_rect(g, rect(col_freq - 100.0f, yy, 100.0f, 18.0f),
                      FONT_MONO, g_pal.text_faint, L"—", TEXT_RIGHT);
            text_rect(g, rect(col_vpp - 80.0f, yy, 80.0f, 18.0f), FONT_MONO,
                      g_pal.text_faint, L"—", TEXT_RIGHT);
            text_rect(g, rect(col_clip - 54.0f, yy, 54.0f, 18.0f), FONT_MONO,
                      g_pal.text_faint, L"—", TEXT_RIGHT);
        }

        if (i < 7)
            line(g, c.x + PAD, yy + 24.0f, c.x + c.w - PAD, yy + 24.0f,
                 g_pal.line_soft, 1.0f);
    }

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    const float rw = r.w - PAD * 2.0f;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint, L"VERDICT");
    text(g, rx, r.y + 30.0f, FONT_CRIT,
         m.clip ? g_pal.bad : g_pal.good, m.clip ? L"FAIL" : L"PASS");

    float yy = r.y + 78.0f;
    label_value(g, rx, yy, rw, L"ACTIVE",
                std::to_wstring(active_count) + L" / 8", g_pal.text);
    label_value(g, rx, yy + 26.0f, rw, L"CLIP",
                m.clip ? L"latched" : L"0", g_pal.text);
    label_value(g, rx, yy + 52.0f, rw, L"CAL", widen(m.cal_confidence),
                g_pal.warn);

    line(g, rx, yy + 84.0f, rx + rw, yy + 84.0f, g_pal.line_soft, 1.0f);
    text(g, rx, yy + 98.0f, FONT_SMALL, g_pal.text_faint,
         L"voltage hidden until calibrated");
}

void draw_repair(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"PLAN");
    text(g, x, y, FONT_BRAND, g_pal.text, L"Tape deck");
    text(g, x, y + 24.0f, FONT_SMALL, g_pal.text_faint,
         L"quick verification · 4 steps");
    button(g, 300, rect(x, y + 58.0f, w, 32.0f), L"RUN TAPE PLAN",
           BTN_PRIMARY);
    text(g, x, y + 104.0f, FONT_SMALL, g_pal.text_dim,
         m.plan_ran ? (m.plan_pass ? L"last run: PASS" : L"last run: FAIL")
                    : L"not run yet");

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"REPAIR PLAN — TAPE", L"thresholds are advisory");

    const float ty = c.y + 56.0f;
    const float col_step = c.x + 24.0f;
    const float col_value = c.x + 300.0f;
    const float col_unit = c.x + 326.0f;
    const float col_result = c.x + 426.0f;

    text(g, col_step, ty, FONT_SMALL, g_pal.text_faint, L"STEP");
    text_rect(g, rect(col_value - 160.0f, ty, 160.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"VALUE", TEXT_RIGHT);
    text(g, col_unit, ty, FONT_SMALL, g_pal.text_faint, L"UNIT");
    text(g, col_result, ty, FONT_SMALL, g_pal.text_faint, L"RESULT");
    line(g, c.x + PAD, ty + 20.0f, c.x + c.w - PAD, ty + 20.0f,
         g_pal.line_soft, 1.0f);

    for (size_t i = 0; i < m.plan.size(); ++i) {
        const NativePlanStep &step = m.plan[i];
        const float yy = ty + 34.0f + static_cast<float>(i) * 40.0f;
        text(g, col_step, yy, FONT_SMALL, g_pal.text, widen(step.label));
        text_rect(g, rect(col_value - 150.0f, yy, 150.0f, 18.0f), FONT_MONO,
                  step.pass ? g_pal.text : g_pal.bad, fmt(step.value, 4),
                  TEXT_RIGHT);
        text(g, col_unit, yy, FONT_SMALL, g_pal.text_faint,
             widen(step.unit));
        text(g, col_result, yy, FONT_SMALL,
             step.pass ? g_pal.good : g_pal.bad,
             step.pass ? L"PASS" : L"FAIL");
    }
    if (!m.plan_ran) {
        text(g, col_step, ty + 40.0f, FONT_SMALL, g_pal.text_faint,
             L"Run the plan to fill in wow / flutter / azimuth / THD.");
    }

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    const float rw = r.w - PAD * 2.0f;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint, L"PLAN VERDICT");
    const bool pass = m.plan_ran ? m.plan_pass : true;
    text(g, rx, r.y + 30.0f, FONT_CRIT,
         m.plan_ran ? (pass ? g_pal.good : g_pal.bad) : g_pal.text_faint,
         m.plan_ran ? (pass ? L"PASS" : L"FAIL") : L"—");

    button(g, 301, rect(rx, r.y + 96.0f, rw, 32.0f), L"EXPORT PDF",
           BTN_NORMAL);
    text(g, rx, r.y + 140.0f, FONT_SMALL, g_pal.text_faint,
         L"writes PDF + JSON + HTML");
    text(g, rx, r.y + 160.0f, FONT_SMALL, g_pal.text_faint,
         m.plan_ran ? L"synthetic tape fixture" : L"no run");
}

void draw_reports(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"OUTPUT");
    text(g, x, y, FONT_SMALL, g_pal.text_dim, L"Application Support");
    text(g, x, y + 20.0f, FONT_SMALL, g_pal.text_faint,
         L"AudioScope / reports");

    y = group(g, x, y + 60.0f, w, L"FORMAT");
    text(g, x, y, FONT_SMALL, g_pal.text_dim, L"PDF · 3 pages · charts");
    text(g, x, y + 22.0f, FONT_SMALL, g_pal.text_faint,
         L"JSON · HTML · provenance");

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"REPORTS", L"most recent first");

    const float ty = c.y + 56.0f;
    text(g, c.x + PAD + 8.0f, ty, FONT_SMALL, g_pal.text_faint, L"FILE");
    text(g, c.x + 330.0f, ty, FONT_SMALL, g_pal.text_faint, L"KIND");
    text(g, c.x + 450.0f, ty, FONT_SMALL, g_pal.text_faint, L"TIME");
    text_rect(g, rect(c.x + c.w - PAD - 60.0f, ty, 60.0f, 16.0f), FONT_SMALL,
              g_pal.text_faint, L"RESULT", TEXT_RIGHT);
    line(g, c.x + PAD, ty + 20.0f, c.x + c.w - PAD, ty + 20.0f,
         g_pal.line_soft, 1.0f);

    if (m.reports.empty()) {
        text(g, c.x + PAD + 8.0f, ty + 42.0f, FONT_SMALL, g_pal.text_faint,
             L"No reports yet. Export one from the repair page or the button →");
    }
    for (size_t i = 0; i < m.reports.size(); ++i) {
        const NativeReport &rep = m.reports[i];
        const float yy = ty + 36.0f + static_cast<float>(i) * 34.0f;
        text_rect(g, rect(c.x + PAD + 8.0f, yy, 290.0f, 18.0f), FONT_MONO,
                  g_pal.text, widen(rep.name), TEXT_LEFT);
        text(g, c.x + 330.0f, yy, FONT_SMALL, g_pal.text_dim,
             widen(rep.kind));
        text(g, c.x + 450.0f, yy, FONT_MONO, g_pal.text_dim,
             widen(rep.time));
        text_rect(g, rect(c.x + c.w - PAD - 60.0f, yy, 60.0f, 18.0f),
                  FONT_SMALL, rep.pass ? g_pal.good : g_pal.warn,
                  rep.pass ? L"PASS" : L"WARN", TEXT_RIGHT);
        if (i + 1 < m.reports.size())
            line(g, c.x + PAD, yy + 24.0f, c.x + c.w - PAD, yy + 24.0f,
                 g_pal.line_soft, 1.0f);
    }

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    const float rw = r.w - PAD * 2.0f;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint, L"EXPORT");
    button(g, 350, rect(rx, r.y + 34.0f, rw, 34.0f), L"EXPORT REPORT",
           BTN_PRIMARY);
    text(g, rx, r.y + 84.0f, FONT_SMALL, g_pal.text_faint,
         L"no network · local files");
    if (!m.last_report.empty()) {
        const size_t slash = m.last_report.find_last_of("/\\");
        const std::string name = slash == std::string::npos
                                     ? m.last_report
                                     : m.last_report.substr(slash + 1);
        text(g, rx, r.y + 120.0f, FONT_SMALL, g_pal.text_dim, L"last:");
        text_rect(g, rect(rx, r.y + 138.0f, rw, 16.0f), FONT_SMALL,
                  g_pal.text, widen(name), TEXT_LEFT);
    }
}

void draw_calibration(gd::Graphics &g, NativeModel &m) {
    Rect l = left_card();
    card(g, l);
    const float x = l.x + PAD;
    const float w = l.w - PAD * 2.0f;
    float y = group(g, x, l.y + PAD, w, L"PROFILE");
    label_value(g, x, y, w, L"ID", widen(m.cal_id), g_pal.text);
    label_value(g, x, y + 26.0f, w, L"DEVICE", widen(m.device), g_pal.text);
    label_value(g, x, y + 52.0f, w, L"CHANNEL", L"CH1", g_pal.text);
    label_value(g, x, y + 78.0f, w, L"GAIN", L"0.0 dB", g_pal.text);

    Rect c = center_card();
    card(g, c);
    title_block(g, c, L"CALIBRATION", L"voltage needs a measured profile");

    const float tx = c.x + PAD + 8.0f;
    const float ty = c.y + 56.0f;
    const float tw = c.w - PAD * 2.0f - 16.0f;
    label_value(g, tx, ty, tw, L"CONFIDENCE", widen(m.cal_confidence),
                g_pal.warn);
    label_value(g, tx, ty + 30.0f, tw, L"FS Vrms",
                m.cal_fs_vrms > 0.0 ? fmt(m.cal_fs_vrms, 4)
                                    : std::wstring(L"—"),
                g_pal.text);
    label_value(g, tx, ty + 60.0f, tw, L"METHOD", L"none", g_pal.text);
    label_value(g, tx, ty + 90.0f, tw, L"BOUND",
                L"device + channel + gain + pad + rate", g_pal.text_faint);

    line(g, c.x + PAD, ty + 130.0f, c.x + c.w - PAD, ty + 130.0f,
         g_pal.line_soft, 1.0f);
    text(g, tx, ty + 146.0f, FONT_SMALL, g_pal.warn,
         L"Uncalibrated: only dBFS is shown. Voltage would be an assumption.");
    text(g, tx, ty + 168.0f, FONT_SMALL, g_pal.text_faint,
         L"Calibrate on this machine with a DMM or reference source; profiles "
         L"are per gain/pad/rate.");

    Rect r = right_card();
    card(g, r);
    const float rx = r.x + PAD;
    text(g, rx, r.y + 14.0f, FONT_SMALL, g_pal.text_faint, L"SAFETY");
    text(g, rx, r.y + 34.0f, FONT_SMALL, g_pal.text_dim,
         L"software cannot protect");
    text(g, rx, r.y + 52.0f, FONT_SMALL, g_pal.text_dim, L"the sound card.");
    text(g, rx, r.y + 78.0f, FONT_SMALL, g_pal.text_faint,
         L"use a protected front end");
    text(g, rx, r.y + 96.0f, FONT_SMALL, g_pal.text_faint,
         L"for unknown or high levels.");
}

void draw_footer(gd::Graphics &g) {
    line(g, 0.0f, H - FOOTER_H, W, H - FOOTER_H, g_pal.line_soft, 1.0f);

    const std::wstring left =
        std::wstring(g_model.capture ? L"INPUT" : L"SYNTHETIC") + L" · " +
        fmt(g_model.fs, 0) + L" Hz · " + std::to_wstring(g_model.channels) +
        L" ch · " + (g_model.hold ? L"HOLD" : L"LIVE");
    text(g, PAD, H - FOOTER_H + 9.0f, FONT_SMALL, g_pal.text_faint, left);
    text_rect(g, rect(0.0f, H - FOOTER_H + 8.0f, W, 16.0f), FONT_SMALL,
              g_pal.text_faint,
              std::wstring(L"BERRYSTUDIO / ") + app_version(), TEXT_CENTER);
    text_rect(g, rect(W - 420.0f, H - FOOTER_H + 8.0f, 404.0f, 16.0f),
              FONT_SMALL, g_pal.text_faint, widen(g_model.message), TEXT_RIGHT);
}

void draw_frame(gd::Graphics &g) {
    fill_rect(g, 0.0f, 0.0f, W, H, g_pal.bg);
    draw_header(g);
    draw_nav(g);
    switch (g_model.page) {
    case audioscope::PAGE_SCOPE: draw_scope(g, g_model); break;
    case audioscope::PAGE_SPECTRUM: draw_spectrum(g, g_model); break;
    case audioscope::PAGE_XY: draw_xy(g, g_model); break;
    case audioscope::PAGE_MEASURE: draw_measure(g, g_model); break;
    case audioscope::PAGE_REPAIR: draw_repair(g, g_model); break;
    case audioscope::PAGE_REPORTS: draw_reports(g, g_model); break;
    case audioscope::PAGE_CALIBRATION: draw_calibration(g, g_model); break;
    default: break;
    }
    draw_footer(g);
}

/* --------------------------------------------------------------- actions */
void set_theme(bool light) {
    g_light = light;
    g_pal = light ? PALETTE_LIGHT : PALETTE_DARK;
    g_model.light = light;
}

void show_error(HWND hwnd, const std::wstring &message) {
    MessageBoxW(hwnd, message.c_str(), L"AudioScope by BerryStudio",
                MB_OK | MB_ICONERROR);
}

void do_capture(HWND hwnd) {
    if (g_model.capture) {
        g_engine.set_capture(&g_model, false, nullptr);
        g_model.message = "capture stopped";
        return;
    }

    const wchar_t *prompt =
        L"Switch from the synthetic source to live input?\n\n"
        L"AudioScope will open the default input device and may ask for "
        L"microphone permission. The UI is currently showing synthetic data. "
        L"Stop capture at any time; the input mode is never switched "
        L"automatically.\n\n"
        L"OK = capture, Cancel = keep synthetic.";
    if (MessageBoxW(hwnd, prompt, L"Switch to live input?",
                    MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK)
        return;

    std::string error;
    if (!g_engine.set_capture(&g_model, true, &error)) {
        show_error(hwnd, L"Cannot open input.\n\n" +
                             widen(error.empty() ? "unknown error" : error));
        return;
    }
    g_model.message = "capturing input";
}

void do_export(HWND hwnd) {
    std::string path;
    std::string error;
    if (!g_engine.export_report(&g_model, &path, &error)) {
        show_error(hwnd, L"Export failed.\n\n" +
                             widen(error.empty() ? "unknown error" : error));
        return;
    }

    const std::wstring wide_path = widen(path);
    const std::wstring message =
        L"Report written to:\n" + wide_path + L"\n\nOpen the PDF now?";
    if (MessageBoxW(hwnd, message.c_str(), L"Report written",
                    MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON1) != IDYES)
        return;

    const HINSTANCE result = ShellExecuteW(hwnd, L"open", wide_path.c_str(),
                                          nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        show_error(hwnd, L"Could not open the PDF.\n\n" + wide_path);
}

void do_about(HWND hwnd) {
    const std::wstring message =
        app_brand() + L"\nVersion " + app_version() +
        L"\n\nNative Win32 measurement workbench.\n"
        L"Synthetic source by default; capture is opt-in.\n"
        L"No WebView, no network access.\n\n"
        L"Copyright 2026 BerryStudio. Apache-2.0.";
    MessageBoxW(hwnd, message.c_str(), L"About AudioScope",
                MB_OK | MB_ICONINFORMATION);
}

void handle_command(HWND hwnd, int id) {
    if (id >= 100 && id < 100 + audioscope::PAGE_COUNT) {
        g_model.page = id - 100;
    } else {
        switch (id) {
        case 200:
            do_capture(hwnd);
            break;
        case 201:
            g_model.hold = !g_model.hold;
            g_model.message = g_model.hold ? "display held" : "display live";
            break;
        case 300:
            g_engine.run_repair(&g_model);
            break;
        case 301:
        case 350:
            do_export(hwnd);
            break;
        case 900:
            set_theme(!g_light);
            break;
        case 901:
            do_about(hwnd);
            break;
        default:
            break;
        }
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

/* --------------------------------------------------------------- input */
int hit_id_at(float x, float y) {
    for (std::vector<HitRegion>::reverse_iterator it = g_hits.rbegin();
         it != g_hits.rend(); ++it) {
        if (hit_test(it->r, x, y)) return it->id;
    }
    return -1;
}

void update_mouse_position(LPARAM lParam) {
    const float sx = static_cast<float>(GET_X_LPARAM(lParam));
    const float sy = static_cast<float>(GET_Y_LPARAM(lParam));
    const float scale = g_scale > 0.0f ? g_scale : 1.0f;
    g_mouse_x = (sx - g_origin_x) / scale;
    g_mouse_y = (sy - g_origin_y) / scale;
}

/* --------------------------------------------------------------- window */
void ensure_backbuffer(int width, int height) {
    if (g_backbuffer != nullptr && g_backbuffer_w == width &&
        g_backbuffer_h == height)
        return;
    delete g_backbuffer;
    g_backbuffer = new gd::Bitmap(width, height, PixelFormat32bppPARGB);
    if (g_backbuffer->GetLastStatus() != gd::Ok) {
        delete g_backbuffer;
        g_backbuffer = nullptr;
        g_backbuffer_w = 0;
        g_backbuffer_h = 0;
        return;
    }
    g_backbuffer_w = width;
    g_backbuffer_h = height;
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (hdc == nullptr) return;

    RECT client;
    if (!GetClientRect(hwnd, &client)) {
        EndPaint(hwnd, &ps);
        return;
    }
    const int cw = client.right - client.left;
    const int ch = client.bottom - client.top;
    if (cw <= 0 || ch <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    ensure_backbuffer(cw, ch);
    if (g_backbuffer == nullptr) {
        RECT full = {0, 0, cw, ch};
        FillRect(hdc, &full,
                 reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        EndPaint(hwnd, &ps);
        return;
    }

    float s = std::min(static_cast<float>(cw) / W,
                       static_cast<float>(ch) / H);
    if (!(s > 0.0f)) s = 1.0f;
    g_scale = s;
    g_origin_x = (static_cast<float>(cw) - W * s) * 0.5f;
    g_origin_y = (static_cast<float>(ch) - H * s) * 0.5f;

    {
        gd::Graphics g(g_backbuffer);
        g.SetSmoothingMode(gd::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(gd::PixelOffsetModeHalf);
        g.SetTextRenderingHint(gd::TextRenderingHintAntiAliasGridFit);
        g.Clear(gd::Color(0xff000000u));

        gd::Matrix transform(s, 0.0f, 0.0f, s, g_origin_x, g_origin_y);
        g.SetTransform(&transform);

        g_hits.clear();
        draw_frame(g);
    }

    gd::Graphics screen(hdc);
    screen.DrawImage(g_backbuffer, 0, 0);
    EndPaint(hwnd, &ps);
}

void on_key_down(HWND hwnd, WPARAM key, LPARAM flags) {
    if ((flags & 0x40000000) != 0) return; /* ignore auto-repeat */
    bool handled = true;

    if (key >= '1' && key <= '7') {
        g_model.page = static_cast<int>(key - '1');
    } else {
        switch (key) {
        case VK_SPACE:
            g_model.hold = !g_model.hold;
            g_model.message = g_model.hold ? "display held" : "display live";
            break;
        case 'C':
        case 'c':
            do_capture(hwnd);
            break;
        case 'R':
        case 'r':
            g_engine.run_repair(&g_model);
            break;
        case 'E':
        case 'e':
            do_export(hwnd);
            break;
        case 'T':
        case 't':
            set_theme(!g_light);
            break;
        default:
            handled = false;
            break;
        }
    }

    if (handled) InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam,
                             LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        paint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = reinterpret_cast<MINMAXINFO *>(lParam);
        if (info != nullptr) {
            info->ptMinTrackSize.x = 900;
            info->ptMinTrackSize.y = 560;
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1) {
            g_engine.tick(&g_model, 1.0 / 30.0);
            if (!IsIconic(hwnd)) InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE:
        update_mouse_position(lParam);
        if (!g_tracking_mouse) {
            TRACKMOUSEEVENT event = {};
            event.cbSize = static_cast<DWORD>(sizeof(event));
            event.dwFlags = TME_LEAVE;
            event.hwndTrack = hwnd;
            TrackMouseEvent(&event);
            g_tracking_mouse = true;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSELEAVE:
        g_tracking_mouse = false;
        g_mouse_x = -10000.0f;
        g_mouse_y = -10000.0f;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
        update_mouse_position(lParam);
        SetFocus(hwnd);
        g_mouse_down = true;
        g_pressed_id = hit_id_at(g_mouse_x, g_mouse_y);
        if (g_pressed_id >= 0) SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONUP: {
        update_mouse_position(lParam);
        const int id = hit_id_at(g_mouse_x, g_mouse_y);
        const bool activate = g_pressed_id >= 0 && id == g_pressed_id;
        const int pressed = g_pressed_id;
        g_mouse_down = false;
        g_pressed_id = -1;
        if (GetCapture() == hwnd) ReleaseCapture();
        if (activate) handle_command(hwnd, pressed);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CAPTURECHANGED:
        g_mouse_down = false;
        g_pressed_id = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_KEYDOWN:
        on_key_down(hwnd, wParam, lParam);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

/* --------------------------------------------------------------- entry */
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
                   int show_command) {
    (void)previous;
    (void)command_line;

    SetProcessDPIAware();

    g_model.platform = as_platform_name();
    g_model.capture_available = true;
    set_theme(false);
    for (int i = 0; i < 12; ++i) g_engine.tick(&g_model, 1.0 / 30.0);

    gd::GdiplusStartupInput startup;
    if (gd::GdiplusStartup(&g_gdiplus_token, &startup, nullptr) != gd::Ok) {
        MessageBoxW(nullptr, L"Could not start GDI+.",
                    L"AudioScope by BerryStudio", MB_OK | MB_ICONERROR);
        return 1;
    }
    init_fonts();
    init_formats();

    WNDCLASSEXW window_class = {};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    window_class.lpszClassName = L"AudioScopeNativeWindow";
    window_class.hIconSm = window_class.hIcon;

    if (RegisterClassExW(&window_class) == 0) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            destroy_formats();
            destroy_fonts();
            gd::GdiplusShutdown(g_gdiplus_token);
            MessageBoxW(nullptr, L"Could not register the window class.",
                        L"AudioScope by BerryStudio", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    RECT frame = {0, 0, static_cast<LONG>(W), static_cast<LONG>(H)};
    AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const int window_w = frame.right - frame.left;
    const int window_h = frame.bottom - frame.top;

    RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN)};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int window_x =
        work.left + ((work.right - work.left) - window_w) / 2;
    const int window_y =
        work.top + ((work.bottom - work.top) - window_h) / 2;

    HWND hwnd = CreateWindowExW(
        0, window_class.lpszClassName, L"AudioScope by BerryStudio",
        WS_OVERLAPPEDWINDOW, window_x < work.left ? work.left : window_x,
        window_y < work.top ? work.top : window_y, window_w, window_h, nullptr,
        nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        destroy_formats();
        destroy_fonts();
        gd::GdiplusShutdown(g_gdiplus_token);
        MessageBoxW(nullptr, L"Could not create the AudioScope window.",
                    L"AudioScope by BerryStudio", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, show_command == 0 ? SW_SHOWNORMAL : show_command);
    UpdateWindow(hwnd);
    SetTimer(hwnd, 1, 33, nullptr);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    delete g_backbuffer;
    g_backbuffer = nullptr;
    g_backbuffer_w = 0;
    g_backbuffer_h = 0;
    destroy_formats();
    destroy_fonts();
    gd::GdiplusShutdown(g_gdiplus_token);
    g_gdiplus_token = 0;

    return static_cast<int>(message.wParam);
}
