/* AudioScope 1.1 — 视图层：精确仪器风格（暖灰石墨 / 单一鼠尾草绿重点色）。
 *
 * 约定：
 *  - 三种排印角色：Label 14（小号 uppercase + tracking）、Value 16、Critical 26；
 *  - 每屏最多两个“关键大数字”，其余是行式读数；
 *  - 主曲线永远是最亮元素，曲线填充只向下；
 *  - 图表内留 32px 安全区，不做发光/霓虹效果。
 */
#include "app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "theme.h"

static void pretty_minus(const char *src, char *dst, size_t n) {
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 4 < n; i++) {
        if (src[i] == '-' && (i == 0 || src[i - 1] == '(' || src[i - 1] == ' ')) {
            dst[o++] = (char)0xE2;
            dst[o++] = (char)0x88;
            dst[o++] = (char)0x92;    /* U+2212 真减号 */
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = 0;
}

static void fnum(char *b, size_t n, double v, int dec, const char *unit) {
    char t[64];
    if (!isfinite(v)) {
        snprintf(b, n, "—");
        return;
    }
    snprintf(t, sizeof(t), "%.*f", dec, v);
    pretty_minus(t, b, n);
    if (unit && *unit) {
        size_t l = strlen(b);
        snprintf(b + l, n - l, " %s", unit);
    }
}

static void micro(UI *u, double x, double y, const char *s, uint32_t col) {
    ui_text(u, x, y, FONT_LABEL, col, s);
}

/* ------------------------------------------------------------------ */
/* 布局游标                                                            */
/* ------------------------------------------------------------------ */
typedef struct {
    UI *u;
    double x, y, w;
} Col;

static void col_begin(Col *c, UI *u, double x, double y, double w) {
    c->u = u;
    c->x = x;
    c->y = y;
    c->w = w;
}

static void col_group(Col *c, const char *title) {
    c->y = ui_group(c->u, c->x, c->y, c->w, title);
    c->y += 2;
}

static void col_label(Col *c, const char *text) {
    micro(c->u, c->x, c->y, text, g_pal.text_faint);
    c->y += 18;
}

static ASRect col_row(Col *c, double h) {
    ASRect r = {c->x, c->y, c->w, h};
    c->y += h + GAP2;
    return r;
}

static ASRect col_pair_left(Col *c, double h, double gap) {
    ASRect r = {c->x, c->y, (c->w - gap) * 0.5, h};
    return r;
}

static ASRect col_pair_right(Col *c, double h, double gap) {
    ASRect r = {c->x + (c->w - gap) * 0.5 + gap, c->y, (c->w - gap) * 0.5, h};
    c->y += h + GAP2;
    return r;
}

static void kv(UI *u, ASRect r, const char *k, const char *v, uint32_t vc) {
    ui_text_left(u, r, FONT_LABEL, g_pal.text_faint, k);
    ui_text_right(u, r, FONT_VALUE, vc, v);
}

static void panel_card(UI *u, ASRect r) { mat_raised(u, r, RADIUS_CARD); }

static void big_number(UI *u, ASRect r, const char *label, const char *value,
                       const char *unit, uint32_t col) {
    micro(u, r.x, r.y, label, g_pal.text_faint);
    ASRect vr = {r.x, r.y + 16, r.w, 30};
    ui_text_left(u, vr, FONT_CRIT, col, value);
    if (unit && *unit) {
        double tw = ui_text_width(u, FONT_CRIT, value);
        ASRect ur = {r.x + tw + 5, r.y + 18, 60, 26};
        ui_text_left(u, ur, FONT_SMALL, g_pal.text_faint, unit);
    }
}

/* ------------------------------------------------------------------ */
/* 图表框：recessed 测量面 + 32px 安全区                                */
/* ------------------------------------------------------------------ */
typedef struct {
    ASRect frame;
    ASRect plot;
    double x0, x1, y0, y1;
} Plot;

static void plot_begin(UI *u, Plot *p, ASRect outer, const char *title,
                       const char *sub, int show_title) {
    mat_recessed(u, outer, RADIUS_CARD);
    ui_fill_round(u, as_rect(outer.x + 1, outer.y + 1, outer.w - 2, outer.h - 2),
                  RADIUS_CARD - 1, g_pal.chart_bg);
    p->frame = outer;
    double top = outer.y + GAP4;
    if (show_title) {
        ASRect tr = {outer.x + GAP6, outer.y + 12, outer.w - 2 * GAP6, 16};
        ui_text_left(u, tr, FONT_LABEL, g_pal.chart_axis, title);
        if (sub && *sub) ui_text_right(u, tr, FONT_MONO, g_pal.chart_axis, sub);
        top = outer.y + 38;
    }
    p->plot = as_rect(outer.x + GAP8, top, outer.w - GAP8 - GAP6,
                      outer.h - (top - outer.y) - GAP8);
    if (p->plot.w < 40 || p->plot.h < 40)
        p->plot = as_rect(outer.x + 8, outer.y + 8, outer.w - 16, outer.h - 16);
}

static void plot_clip_on(UI *u, Plot *p) {
    CGContextSaveGState(u->ctx);
    CGContextClipToRect(u->ctx,
                        CGRectMake(p->plot.x, p->plot.y, p->plot.w, p->plot.h));
}

static void plot_clip_off(UI *u) { CGContextRestoreGState(u->ctx); }

static inline double px(Plot *p, double x) {
    double t = (p->x1 > p->x0) ? (x - p->x0) / (p->x1 - p->x0) : 0;
    return p->plot.x + AS_CLAMP(t, -0.02, 1.02) * p->plot.w;
}
static inline double py(Plot *p, double y) {
    double t = (p->y1 > p->y0) ? (y - p->y0) / (p->y1 - p->y0) : 0;
    return p->plot.y + (1.0 - AS_CLAMP(t, -0.02, 1.02)) * p->plot.h;
}

static void plot_hgrid(UI *u, Plot *p, int n, int labels) {
    char buf[32];
    double step = (p->y1 - p->y0) / n;
    double mag = fabs(step);
    int dec = 0;
    if (mag < 10.0) dec = 1;
    if (mag < 1.0) dec = 2;
    if (mag < 0.1) dec = 3;
    for (int i = 0; i <= n; i++) {
        double v = p->y0 + step * (double)i;
        double y = py(p, v);
        ui_line(u, p->plot.x, y, p->plot.x + p->plot.w, y, g_pal.chart_grid, 1);
        if (!labels) continue;
        if (fabs(v) < step * 0.001) v = 0.0;      /* 去掉 -0 */
        snprintf(buf, sizeof(buf), "%.*f", dec, v);
        ASRect lr = {p->plot.x - 46, y - 8, 38, 16};
        ui_text_right(u, lr, FONT_MONO, g_pal.chart_axis, buf);
    }
}

static void plot_vgrid(UI *u, Plot *p, int n, const char *const *labels) {
    for (int i = 0; i <= n; i++) {
        double x = p->plot.x + p->plot.w * (double)i / n;
        ui_line(u, x, p->plot.y, x, p->plot.y + p->plot.h, g_pal.chart_grid, 1);
        if (labels && labels[i]) {
            ASRect lr = {x - 32, p->plot.y + p->plot.h + 4, 64, 14};
            ui_text_center(u, lr, FONT_MONO, g_pal.chart_axis, labels[i]);
        }
    }
}

/* 取“好看”的刻度：step ∈ {1,2,2.5,5}×10^n，返回刻度数 */
static int nice_grid(double want, double *step) {
    static const double cand[4] = {1.0, 2.0, 2.5, 5.0};
    double w = fabs(want);
    if (w < 1e-9) w = 1.0;
    double mag = pow(10.0, floor(log10(w)));
    int divs = 4;
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < 4; i++) {
            double s = cand[i] * mag;
            divs = (int)ceil(w / s);
            if (divs >= 2 && divs <= 8) {
                *step = s;
                return divs;
            }
        }
        mag *= 0.5;
    }
    *step = w / 4.0;
    return 4;
}

static void polyline(UI *u, Plot *p, const float *y, int n, double dx,
                     uint32_t col, double width) {
    if (n < 2) return;
    CGColorRef c = CGColorCreateGenericRGB(((col >> 24) & 0xff) / 255.0,
                                           ((col >> 16) & 0xff) / 255.0,
                                           ((col >> 8) & 0xff) / 255.0,
                                           (col & 0xff) / 255.0);
    CGContextSetStrokeColorWithColor(u->ctx, c);
    CGContextSetLineWidth(u->ctx, width);
    CGContextSetLineJoin(u->ctx, kCGLineJoinRound);
    CGContextSetLineCap(u->ctx, kCGLineCapRound);
    CGContextBeginPath(u->ctx);
    CGContextMoveToPoint(u->ctx, p->plot.x, py(p, y[0]));
    for (int i = 1; i < n; i++)
        CGContextAddLineToPoint(u->ctx, p->plot.x + (double)i * dx,
                                py(p, y[i]));
    CGContextStrokePath(u->ctx);
    CGColorRelease(c);
}

/* 向下填充：逐列竖条，避免尖峰处多边形自交产生“空心” */
static void fill_down(UI *u, Plot *p, const float *y, int n, double dx) {
    if (n < 2) return;
    CGColorRef c = CGColorCreateGenericRGB(
        ((g_pal.curve_fill >> 24) & 0xff) / 255.0,
        ((g_pal.curve_fill >> 16) & 0xff) / 255.0,
        ((g_pal.curve_fill >> 8) & 0xff) / 255.0, 0.16);
    CGContextSetFillColorWithColor(u->ctx, c);
    double bottom = p->plot.y + p->plot.h;
    double barw = (dx > 1.0 ? dx : 1.0) + 0.6;
    CGContextBeginPath(u->ctx);
    for (int i = 0; i < n; i++) {
        double top = py(p, y[i]);
        if (top < p->plot.y) top = p->plot.y;
        if (top >= bottom) continue;
        double x = p->plot.x + (double)i * dx;
        CGContextAddRect(u->ctx, CGRectMake(x, top, barw, bottom - top));
    }
    CGContextFillPath(u->ctx);
    CGColorRelease(c);
}


/* ------------------------------------------------------------------ */
/* 时域波形                                                            */
/* ------------------------------------------------------------------ */
static void scope_chart(App *a, UI *u, ASRect r) {
    Plot p;
    char sub[96];
    snprintf(sub, sizeof(sub), "%.3g s/div · %.3g V/div · %d Sa",
             a->time_div, a->volt_div, (int)a->fs);
    plot_begin(u, &p, r, "时域", sub, 1);

    double half = a->volt_div * 4.0;
    p.x0 = 0;
    p.x1 = a->scope.span > 0 ? a->scope.span : 1e-3;
    p.y0 = a->v_off - half;
    p.y1 = a->v_off + half;

    /* 网格：10 × 8（与 8 格垂直刻度一致） */
    plot_hgrid(u, &p, 8, 1);
    plot_vgrid(u, &p, 10, NULL);
    /* 0 V 参考线稍亮 */
    double zy = py(&p, 0.0);
    if (zy > p.plot.y && zy < p.plot.y + p.plot.h)
        ui_line(u, p.plot.x, zy, p.plot.x + p.plot.w, zy,
                as_alpha(g_pal.chart_axis, 0.55), 1);

    plot_clip_on(u, &p);
    int n = a->scope.n;
    double dx = (n > 1) ? p.plot.w / (double)(n - 1) : 0;
    if (a->show_ch[0] && n > 1) fill_down(u, &p, a->scope.ch1, n, dx);
    if (a->show_ch[1] && n > 1)
        polyline(u, &p, a->scope.ch2, n, dx, g_pal.curve_alt, 1.5);
    if (a->math && n > 1)
        polyline(u, &p, a->scope.math, n, dx, g_pal.curve_math, 1.4);
    if (a->show_ch[0] && n > 1)
        polyline(u, &p, a->scope.ch1, n, dx, g_pal.curve_main, 1.8);

    /* 触发电平：虚线只画到右端标记处，避免和 0 V 参考线叠在一起 */
    if (a->trig_src >= 0 && fabs(a->trig_level) > (p.y1 - p.y0) * 0.01) {
        double ty = py(&p, a->trig_level);
        const double dash[2] = {4, 4};
        ui_line_dash(u, p.plot.x, ty, p.plot.x + p.plot.w, ty,
                     as_alpha(g_pal.accent, 0.6), 1, dash, 2);
    }
    plot_clip_off(u);

    /* 削波标记：贴边的小竖线 */
    if (a->tstat[0].clipped || a->tstat[1].clipped) {
        for (int i = 0; i < 2; i++) {
            if (!a->tstat[i].clipped) continue;
            uint32_t c = g_pal.bad;
            ui_fill(u, as_rect(p.plot.x, p.plot.y + (i ? p.plot.h - 5 : 0),
                               p.plot.w, 2),
                    as_alpha(c, 0.55));
        }
        ui_text_right(u, as_rect(p.plot.x, p.plot.y + 2, p.plot.w, 16),
                      FONT_SMALL, g_pal.bad, "CLIP");
    }

    /* 轴单位 */
    char tu[32];
    fnum(tu, sizeof(tu), p.x1 * 1000.0, 2, "ms");
    ui_text_right(u, as_rect(p.plot.x, p.plot.y + p.plot.h + 4, p.plot.w, 14),
                  FONT_MONO, g_pal.chart_axis, tu);
}

/* 左侧：采集 / 触发 / 时基 / 垂直 */
static void scope_left(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP2, r.w - GAP4 * 2);

    col_group(&c, "采集");
    if (ui_button(u, 1, col_row(&c, CTRL_H + 2),
                  a->running ? "停止采集" : "开始采集",
                  a->running ? BTN_NORMAL : BTN_PRIMARY))
        a->running ? app_stop(a) : app_start(a);

    /* 设备：用同一套循环控件，名字来自实际设备表 */
    static const char *ins[64], *outs[64];
    int ni = 0, no = 0, ii = 0, oi = 0;
    for (int i = 0; i < a->ndev && ni < 63; i++) {
        if (a->devs[i].is_input) {
            if ((int)a->devs[i].id == a->in_dev) ii = ni;
            ins[ni++] = a->devs[i].name;
        }
    }
    for (int i = 0; i < a->ndev && no < 63; i++) {
        if (a->devs[i].is_output) {
            if ((int)a->devs[i].id == a->out_dev) oi = no;
            outs[no++] = a->devs[i].name;
        }
    }
    col_label(&c, "输入设备");
    if (ni > 0 && ui_cycle(u, 10, col_row(&c, CTRL_H), ins, ni, &ii)) {
        int k = 0;
        for (int i = 0; i < a->ndev; i++)
            if (a->devs[i].is_input && k++ == ii) {
                app_apply_devices(a, (int)a->devs[i].id, a->out_dev);
                break;
            }
    }
    col_label(&c, "输出设备");
    if (no > 0 && ui_cycle(u, 12, col_row(&c, CTRL_H), outs, no, &oi)) {
        int k = 0;
        for (int i = 0; i < a->ndev; i++)
            if (a->devs[i].is_output && k++ == oi) {
                app_apply_devices(a, a->in_dev, (int)a->devs[i].id);
                break;
            }
    }

    static const char *RATE_LBL[] = {"44.1k", "48k", "88.2k", "96k", "176.4k", "192k"};
    col_label(&c, "采样率");
    int ri = a->rate_idx;
    if (ui_cycle(u, 14, col_row(&c, CTRL_H), RATE_LBL, 6, &ri)) {
        a->rate_idx = ri;
        if (a->running) {
            app_stop(a);
            app_start(a);
        }
    }

    col_group(&c, "触发");
    static const char *TRIG_SRC[] = {"CH1", "CH2", "自由"};
    int ts = a->trig_src < 0 ? 2 : a->trig_src;
    if (ui_cycle(u, 20, col_row(&c, CTRL_H), TRIG_SRC, 3, &ts))
        a->trig_src = (ts == 2) ? -1 : ts;
    static const char *EDGE[] = {"上升沿", "下降沿"};
    int ed = a->trig_edge;
    if (ui_cycle(u, 22, col_row(&c, CTRL_H), EDGE, 2, &ed)) a->trig_edge = ed;
    col_label(&c, "触发电平");
    ui_field(u, 24, col_row(&c, CTRL_H), &a->trig_level, -5.0, 5.0, "V", 2);

    col_group(&c, "时基 / 垂直");
    ASRect t1 = col_pair_left(&c, CTRL_H, GAP2);
    ASRect v1 = col_pair_right(&c, CTRL_H, GAP2);
    ui_field(u, 30, t1, &a->time_div, 1e-6, 5.0, "s", 4);
    ui_field(u, 32, v1, &a->volt_div, 1e-4, 50.0, "V", 3);
    col_label(&c, "偏移");
    ui_field(u, 34, col_row(&c, CTRL_H), &a->v_off, -20.0, 20.0, "V", 2);

    col_group(&c, "通道");
    ASRect ch1 = col_pair_left(&c, CTRL_H, GAP2);
    ASRect ch2 = col_pair_right(&c, CTRL_H, GAP2);
    ui_toggle(u, 40, ch1, "CH1", &a->show_ch[0]);
    ui_toggle(u, 42, ch2, "CH2", &a->show_ch[1]);
    ASRect cp1 = col_pair_left(&c, CTRL_H, GAP2);
    ASRect cp2 = col_pair_right(&c, CTRL_H, GAP2);
    static const char *COUP[] = {"AC", "DC"};
    int cpl = a->coupling;
    if (ui_cycle(u, 44, cp1, COUP, 2, &cpl)) a->coupling = cpl;
    static const char *MATH[] = {"无", "A+B", "A−B"};
    int mth = a->math;
    if (ui_cycle(u, 46, cp2, MATH, 3, &mth)) a->math = mth;
}


/* 右侧：两个关键数字 + 行式读数 */
static void scope_right(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP4, r.w - GAP4 * 2);
    char v[48], u1[24], v2[48];

    TimeStats *t = &a->tstat[0];
    fnum(v, sizeof(v), t->vpp, 3, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "峰峰值 · CH1", v, "V",
               g_pal.curve_main);
    c.y += 54;
    fnum(v2, sizeof(v2), t->freq, t->freq >= 1000 ? 1 : 2, "");
    if (t->freq >= 1000) {
        fnum(v2, sizeof(v2), t->freq / 1000.0, 3, "");
        snprintf(u1, sizeof(u1), "kHz");
    } else {
        snprintf(u1, sizeof(u1), "Hz");
    }
    big_number(u, as_rect(c.x, c.y, c.w, 46), "频率 · CH1", v2, u1,
               g_pal.text);
    c.y += 54;

    c.y = ui_group(u, c.x, c.y, c.w, "读数") + 4;
    char k2[48];
    fnum(v, sizeof(v), t->ac_rms, 3, "V");   fnum(k2, sizeof(k2), t->crest, 2, "");
    kv(u, col_row(&c, 22), "有效值", v, g_pal.text);
    kv(u, col_row(&c, 22), "峰值因数", k2, g_pal.text_dim);
    {
        char a1[32], a2[32], both[72];
        fnum(a1, sizeof(a1), t->vmax, 3, "");
        fnum(a2, sizeof(a2), t->vmin, 3, "");
        snprintf(both, sizeof(both), "%s / %s", a1, a2);
        kv(u, col_row(&c, 22), "最大 / 最小", both, g_pal.text_dim);
    }
    fnum(v, sizeof(v), t->duty * 100.0, 1, "%");
    kv(u, col_row(&c, 22), "占空比", v, g_pal.text_dim);
    fnum(v, sizeof(v), a->scope.span * 1000.0, 2, "ms");
    kv(u, col_row(&c, 22), "记录长度", v, g_pal.text_dim);
    fnum(v, sizeof(v), (double)a->eng.xruns, 0, "");
    kv(u, col_row(&c, 22), "溢出", v,
       a->eng.xruns ? g_pal.warn : g_pal.text_dim);

    c.y = ui_group(u, c.x, c.y + 8, c.w, "削波") + 4;
    int clip = a->tstat[0].clipped || a->tstat[1].clipped;
    kv(u, col_row(&c, 22), "状态", clip ? "削波" : "正常",
       clip ? g_pal.bad : g_pal.good);
}

/* ------------------------------------------------------------------ */
/* 频谱                                                                */
/* ------------------------------------------------------------------ */
static void fft_chart(App *a, UI *u, ASRect r) {
    Plot p;
    char sub[64];
    snprintf(sub, sizeof(sub), "%s · %d pt", as_win_name(a->fft_win),
             as_fft_size(a->fft_size_idx));
    plot_begin(u, &p, r, "频谱", sub, 1);
    p.x0 = log10(a->spec_low);
    p.x1 = log10(a->spec_high);
    /* dB 轴按 20 dB 一格，这是频谱分析仪的习惯刻度 */
    int gdiv = 7;
    p.y0 = -20.0 * (double)gdiv;
    p.y1 = 0;
    plot_hgrid(u, &p, gdiv, 1);
    static const char *FL[10] = {"20", "50", "100", "200", "500",
                                 "1k", "2k", "5k", "10k", "20k"};
    for (int i = 0; i < 10; i++) {
        double f = atof(FL[i]) * (FL[i][strlen(FL[i]) - 1] == 'k' ? 1000.0 : 1.0);
        double x = px(&p, log10(f));
        if (x < p.plot.x - 1 || x > p.plot.x + p.plot.w + 1) continue;
        ui_line(u, x, p.plot.y, x, p.plot.y + p.plot.h, g_pal.chart_grid, 1);
        ASRect lr = {x - 26, p.plot.y + p.plot.h + 4, 52, 14};
        ui_text_center(u, lr, FONT_MONO, g_pal.chart_axis, FL[i]);
    }

    plot_clip_on(u, &p);
    int n = a->spec.n;
    double dx = (n > 1) ? p.plot.w / (double)(n - 1) : 0;
    if (a->peak_hold) {
        float tmp[SPEC_COLS];
        for (int i = 0; i < n; i++) tmp[i] = a->spec.db[2][i];
        polyline(u, &p, tmp, n, dx, as_alpha(g_pal.curve_hold, 0.55), 1.0);
    }
    if (a->show_ch[1]) polyline(u, &p, a->spec.db[1], n, dx, g_pal.curve_alt, 1.4);
    if (a->show_ch[0]) {
        fill_down(u, &p, a->spec.db[0], n, dx);
        polyline(u, &p, a->spec.db[0], n, dx, g_pal.curve_main, 1.8);
    }
    /* 读数标记 */
    for (int i = 0; i < a->spec.mark_n; i++) {
        double x = px(&p, a->spec.marks_f[i]);
        if (x < p.plot.x || x > p.plot.x + p.plot.w) continue;
        ui_line(u, x, py(&p, a->spec.marks_db[i]) - 3, x,
                py(&p, a->spec.marks_db[i]) + 3, g_pal.curve_warn, 1.2);
    }
    plot_clip_off(u);
}

static void fft_left(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP2, r.w - GAP4 * 2);
    col_group(&c, "FFT");
    static const char *SZ[] = {"1k", "2k", "4k", "8k", "16k", "32k", "64k", "128k"};
    int si = a->fft_size_idx;
    if (ui_cycle(u, 200, col_row(&c, CTRL_H), SZ, 8, &si)) {
        a->fft_size_idx = si;
        app_ensure_fft_pub(a);
    }
    static const char *WN[] = {"BH4", "Hann", "Hamming", "Blackman", "Flat-top", "Rect"};
    int wi = a->fft_win;
    if (ui_cycle(u, 202, col_row(&c, CTRL_H), WN, 6, &wi)) a->fft_win = wi;
    col_label(&c, "平均");
    ui_field(u, 204, col_row(&c, CTRL_H), &a->fft_avg, 1.0, 64.0, "×", 0);
    ui_toggle(u, 206, col_row(&c, CTRL_H), "峰值保持", &a->peak_hold);

    col_group(&c, "显示范围");
    col_label(&c, "下限");
    ui_field(u, 210, col_row(&c, CTRL_H), &a->spec_low, 5.0, 1000.0, "Hz", 0);
    col_label(&c, "上限");
    ui_field(u, 212, col_row(&c, CTRL_H), &a->spec_high, 1000.0, 96000.0, "Hz", 0);

    col_group(&c, "瀑布");
    static const char *WF[] = {"512", "1k", "2k", "4k", "8k"};
    int wfs = a->wf_fft_idx;
    if (ui_cycle(u, 220, col_row(&c, CTRL_H), WF, 5, &wfs)) a->wf_fft_idx = wfs;
    col_label(&c, "跨度");
    ui_field(u, 222, col_row(&c, CTRL_H), &a->wf_span, 0.5, 30.0, "s", 1);
    col_label(&c, "低 / 高");
    ASRect w1 = col_pair_left(&c, CTRL_H, GAP2);
    ASRect w2 = col_pair_right(&c, CTRL_H, GAP2);
    ui_field(u, 224, w1, &a->wf_low, -140.0, -20.0, "dB", 0);
    ui_field(u, 226, w2, &a->wf_high, -80.0, 0.0, "dB", 0);
}


static void fft_right(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP4, r.w - GAP4 * 2);
    char v[48], v2[48];

    ToneMetrics *t = &a->tone[0];
    int ok = a->tone_valid[0];
    double fpk = ok ? t->freq : 0;
    if (fpk >= 1000) fnum(v, sizeof(v), fpk / 1000.0, 3, "");
    else fnum(v, sizeof(v), fpk, 1, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "峰值频率",
               ok ? v : "—", fpk >= 1000 ? "kHz" : "Hz", g_pal.curve_main);
    c.y += 54;
    fnum(v2, sizeof(v2), ok ? t->thdn * 100.0 : NAN, 3, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "THD+N",
               ok ? v2 : "—", "%", ok ? g_pal.text : g_pal.text_faint);
    c.y += 54;

    c.y = ui_group(u, c.x, c.y, c.w, "分量") + 4;
    fnum(v, sizeof(v), ok ? t->amp_db : NAN, 2, "dBFS");
    kv(u, col_row(&c, 22), "基波", v, g_pal.text);
    fnum(v, sizeof(v), ok ? t->thd * 100.0 : NAN, 3, "%");
    kv(u, col_row(&c, 22), "THD", v, g_pal.text_dim);
    fnum(v, sizeof(v), ok ? t->snr : NAN, 1, "dB");
    kv(u, col_row(&c, 22), "SNR", v, g_pal.text_dim);
    fnum(v, sizeof(v), ok ? t->sinad : NAN, 1, "dB");
    kv(u, col_row(&c, 22), "SINAD", v, g_pal.text_dim);
    fnum(v, sizeof(v), ok ? t->enob : NAN, 1, "bit");
    kv(u, col_row(&c, 22), "ENOB", v, g_pal.text_dim);
    fnum(v, sizeof(v), ok ? t->sfdr : NAN, 1, "dB");
    kv(u, col_row(&c, 22), "SFDR", v, g_pal.text_dim);
    fnum(v, sizeof(v), a->spec_peak && a->spec_peak[0] > -170.0f ? a->spec_peak[0] : NAN, 1, "dB");
    kv(u, col_row(&c, 22), "峰值保持", v, g_pal.text_dim);
}

/* ------------------------------------------------------------------ */
/* 频率响应                                                            */
/* ------------------------------------------------------------------ */
static void bode_chart(App *a, UI *u, ASRect r) {
    BodeTrace *b = &a->bode;
    /* 上下两个绘图区：幅频（上）/ 相频（下） */
    ASRect top = r;
    top.h = r.h * 0.58;
    ASRect bot = r;
    bot.y = r.y + top.h + GAP2;
    bot.h = r.h - top.h - GAP2;

    Plot pm, pp;
    plot_begin(u, &pm, top, "幅频 dB", NULL, 1);
    plot_begin(u, &pp, bot, "相频 °", NULL, 1);
    for (int i = 0; i < 2; i++) {
        Plot *p = i ? &pp : &pm;
        p->x0 = log10(a->bode_f1);
        p->x1 = log10(a->bode_f2);
    }
    pm.y0 = -40;
    pm.y1 = 10;
    pp.y0 = -180;
    pp.y1 = 90;

    plot_hgrid(u, &pm, 5, 1);
    plot_hgrid(u, &pp, 3, 1);
    for (int i = 0; i < 2; i++) {
        Plot *p = i ? &pp : &pm;
        static const char *FL[7] = {"20", "100", "500", "1k", "5k", "10k", "20k"};
        for (int k = 0; k < 7; k++) {
            double f = atof(FL[k]) * (strchr(FL[k], 'k') ? 1000.0 : 1.0);
            double x = px(p, log10(f));
            if (x < p->plot.x - 1 || x > p->plot.x + p->plot.w + 1) continue;
            ui_line(u, x, p->plot.y, x, p->plot.y + p->plot.h, g_pal.chart_grid, 1);
            if (i == 1) {
                ASRect lr = {x - 26, p->plot.y + p->plot.h + 4, 52, 14};
                ui_text_center(u, lr, FONT_MONO, g_pal.chart_axis, FL[k]);
            }
        }
    }

    int n = b->n;
    if (n < 2) return;
    float g[BODE_MAX], ph[BODE_MAX];
    for (int i = 0; i < n; i++) {
        g[i] = (float)b->g[i];
        ph[i] = (float)b->ph[i];
    }
    plot_clip_on(u, &pm);
    /* 目标：0 dB 参考线 */
    ui_line(u, pm.plot.x, py(&pm, 0), pm.plot.x + pm.plot.w, py(&pm, 0),
            as_alpha(g_pal.chart_axis, 0.5), 1);
    {
        /* 折线按对数频率绘制 */
        CGColorRef c = CGColorCreateGenericRGB(
            ((g_pal.curve_main >> 24) & 0xff) / 255.0,
            ((g_pal.curve_main >> 16) & 0xff) / 255.0,
            ((g_pal.curve_main >> 8) & 0xff) / 255.0, 1.0);
        CGContextSetStrokeColorWithColor(u->ctx, c);
        CGContextSetLineWidth(u->ctx, 1.8);
        CGContextSetLineJoin(u->ctx, kCGLineJoinRound);
        CGContextBeginPath(u->ctx);
        for (int i = 0; i < n; i++) {
            double x = px(&pm, log10(b->f[i]));
            double y = py(&pm, g[i]);
            if (i == 0) CGContextMoveToPoint(u->ctx, x, y);
            else CGContextAddLineToPoint(u->ctx, x, y);
        }
        CGContextStrokePath(u->ctx);
        CGColorRelease(c);
    }
    plot_clip_off(u);

    plot_clip_on(u, &pp);
    {
        CGColorRef c = CGColorCreateGenericRGB(
            ((g_pal.curve_alt >> 24) & 0xff) / 255.0,
            ((g_pal.curve_alt >> 16) & 0xff) / 255.0,
            ((g_pal.curve_alt >> 8) & 0xff) / 255.0, 1.0);
        CGContextSetStrokeColorWithColor(u->ctx, c);
        CGContextSetLineWidth(u->ctx, 1.5);
        CGContextBeginPath(u->ctx);
        for (int i = 0; i < n; i++) {
            double x = px(&pp, log10(b->f[i]));
            double y = py(&pp, ph[i]);
            if (i == 0) CGContextMoveToPoint(u->ctx, x, y);
            else CGContextAddLineToPoint(u->ctx, x, y);
        }
        CGContextStrokePath(u->ctx);
        CGColorRelease(c);
    }
    plot_clip_off(u);
}


static void bode_left(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP2, r.w - GAP4 * 2);
    col_group(&c, "扫频");
    col_label(&c, "起始");
    ui_field(u, 300, col_row(&c, CTRL_H), &a->bode_f1, 5.0, 20000.0, "Hz", 0);
    col_label(&c, "终止");
    ui_field(u, 302, col_row(&c, CTRL_H), &a->bode_f2, 100.0, 96000.0, "Hz", 0);
    col_label(&c, "点数");
    double pts = a->bode_points;
    if (ui_field(u, 304, col_row(&c, CTRL_H), &pts, 8, 400, "pt", 0))
        a->bode_points = (int)pts;

    if (ui_button(u, 306, col_row(&c, CTRL_H + 2), "开始扫频", BTN_PRIMARY)) {
        a->bode_active = 1;
        a->bode_idx = 0;
        a->bode.n = 0;
        a->bode_next = 0;
        app_set_message(a, "开始扫频：%d 点", a->bode_points);
    }
    if (ui_button(u, 308, col_row(&c, CTRL_H), "停止", BTN_GHOST))
        a->bode_active = 0;

    col_group(&c, "说明");
    ui_text_left(u, col_row(&c, 16), FONT_SMALL, g_pal.text_faint,
                 "输出 → 被测电路 → 输入。");
    ui_text_left(u, col_row(&c, 16), FONT_SMALL, g_pal.text_faint,
                 "每点先稳定再取值，可导出 CSV。");
    c.y += 4;
    ui_text_left(u, col_row(&c, 16), FONT_SMALL, g_pal.text_faint,
                 "建议先在闭环测试页做直连校准。");
}

static void bode_right(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    char tmp[32];
    col_begin(&c, u, r.x + GAP4, r.y + GAP4, r.w - GAP4 * 2);
    char v[48];
    BodeTrace *b = &a->bode;

    double gmin = 1e9, gmax = -1e9, pmin = 1e9, pmax = -1e9;
    for (int i = 0; i < b->n; i++) {
        if (b->g[i] < gmin) gmin = b->g[i];
        if (b->g[i] > gmax) gmax = b->g[i];
        if (b->ph[i] < pmin) pmin = b->ph[i];
        if (b->ph[i] > pmax) pmax = b->ph[i];
    }
    fnum(v, sizeof(v), b->n ? gmax - gmin : NAN, 2, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "幅频峰谷差", v, "dB",
               g_pal.curve_main);
    c.y += 54;
    fnum(v, sizeof(v), b->n ? pmax - pmin : NAN, 0, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "相位范围", v, "°", g_pal.text);
    c.y += 54;

    c.y = ui_group(u, c.x, c.y, c.w, "扫频状态") + 4;
    snprintf(tmp, sizeof(tmp), "%d", b->n);
    kv(u, col_row(&c, 22), "点数", b->n ? tmp : "—", g_pal.text);
    snprintf(tmp, sizeof(tmp), "%d / %d", a->bode_idx, a->bode_points);
    kv(u, col_row(&c, 22), "进度", tmp, g_pal.text_dim);
    snprintf(tmp, sizeof(tmp), "%d", a->bode_active ? 1 : 0);
    kv(u, col_row(&c, 22), "运行", a->bode_active ? "是" : "否",
       a->bode_active ? g_pal.accent : g_pal.text_dim);
    kv(u, col_row(&c, 22), "导出", "CSV", g_pal.text_dim);
    ui_bar(u, col_row(&c, 6),
           a->bode_points ? (double)a->bode_idx / a->bode_points : 0,
           g_pal.accent);
}

/* ------------------------------------------------------------------ */
/* 闭环测试                                                            */
/* ------------------------------------------------------------------ */
static void loop_chart(App *a, UI *u, ASRect r) {
    Loopback *lb = &a->lb;
    Plot p;
    char sub[96];
    snprintf(sub, sizeof(sub), "%d 点 · 容差 ±%.2f dB", lb->cfg.points,
             lb->cfg.tol_db);
    plot_begin(u, &p, r, "频响 · 闭环", sub, 1);
    p.x0 = log10(lb->nfreq ? lb->freqs[0] : 20.0);
    p.x1 = log10(lb->nfreq ? lb->freqs[lb->nfreq - 1] : 20000.0);
    /* 容差带：0 dB ± tol；纵轴取“好看”的整数刻度 */
    double span = AS_MAX(1.5, lb->cfg.tol_db * 3.0);
    for (int i = 0; i < lb->nfr; i++) {
        double d = fabs(lb->fr[i].gain_db);
        if (d > span) span = d;
    }
    double step = 5.0;
    int divs = nice_grid(span * 2.0, &step);
    span = step * (double)divs * 0.5;
    p.y0 = -span;
    p.y1 = span;

    plot_hgrid(u, &p, divs, 1);
    static const char *FL[7] = {"20", "100", "500", "1k", "5k", "10k", "20k"};
    for (int k = 0; k < 7; k++) {
        double f = atof(FL[k]) * (strchr(FL[k], 'k') ? 1000.0 : 1.0);
        double x = px(&p, log10(f));
        if (x < p.plot.x - 1 || x > p.plot.x + p.plot.w + 1) continue;
        ui_line(u, x, p.plot.y, x, p.plot.y + p.plot.h, g_pal.chart_grid, 1);
        ASRect lr = {x - 26, p.plot.y + p.plot.h + 4, 52, 14};
        ui_text_center(u, lr, FONT_MONO, g_pal.chart_axis, FL[k]);
    }

    /* 容差带：0 dB ± tol */
    double yt = py(&p, lb->cfg.tol_db), yb = py(&p, -lb->cfg.tol_db);
    ui_fill(u, as_rect(p.plot.x, yt, p.plot.w, yb - yt),
            as_alpha(g_pal.accent, 0.10));
    ui_line(u, p.plot.x, py(&p, 0), p.plot.x + p.plot.w, py(&p, 0),
            as_alpha(g_pal.chart_axis, 0.55), 1);

    if (lb->nfr > 1) {
        plot_clip_on(u, &p);
        CGColorRef c = CGColorCreateGenericRGB(
            ((g_pal.curve_main >> 24) & 0xff) / 255.0,
            ((g_pal.curve_main >> 16) & 0xff) / 255.0,
            ((g_pal.curve_main >> 8) & 0xff) / 255.0, 1.0);
        CGContextSetStrokeColorWithColor(u->ctx, c);
        CGContextSetLineWidth(u->ctx, 1.8);
        CGContextSetLineJoin(u->ctx, kCGLineJoinRound);
        CGContextBeginPath(u->ctx);
        for (int i = 0; i < lb->nfr; i++) {
            double x = px(&p, log10(lb->fr[i].f));
            double y = py(&p, lb->fr[i].gain_db);
            if (i == 0) CGContextMoveToPoint(u->ctx, x, y);
            else CGContextAddLineToPoint(u->ctx, x, y);
        }
        CGContextStrokePath(u->ctx);
        CGColorRelease(c);
        /* 数据点标记 */
        for (int i = 0; i < lb->nfr; i++) {
            double x = px(&p, log10(lb->fr[i].f));
            double y = py(&p, lb->fr[i].gain_db);
            ui_dot(u, x, y, 1.6, g_pal.chart_bg);
            ui_dot(u, x, y, 1.1, g_pal.curve_main);
        }
        plot_clip_off(u);
    } else {
        ui_text_center(u, p.plot, FONT_LABEL, g_pal.chart_axis,
                       "尚无数据：连接输出→被测→输入，然后点「开始测试」");
    }
}


static void loop_left(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP2, r.w - GAP4 * 2);
    LbConfig *cfg = &a->lbcfg;
    int running = a->lb.state == LB_RUN;

    col_group(&c, "激励");
    col_label(&c, "电平");
    ui_field(u, 400, col_row(&c, CTRL_H), &cfg->level_dbfs, -60.0, -1.0,
             "dBFS", 1);
    col_label(&c, "频率起点");
    ui_field(u, 402, col_row(&c, CTRL_H), &cfg->f1, 5.0, 2000.0, "Hz", 0);
    col_label(&c, "频率终点");
    ui_field(u, 404, col_row(&c, CTRL_H), &cfg->f2, 1000.0, 96000.0, "Hz", 0);
    col_label(&c, "点数 / 容差");
    double pts = cfg->points;
    ASRect pr = col_pair_left(&c, CTRL_H, GAP2);
    ASRect tr = col_pair_right(&c, CTRL_H, GAP2);
    if (ui_field(u, 406, pr, &pts, 4, AN_MAX_FR, "pt", 0))
        cfg->points = (int)pts;
    ui_field(u, 408, tr, &cfg->tol_db, 0.1, 6.0, "dB", 2);

    col_group(&c, "项目");
    ui_toggle(u, 420, col_row(&c, CTRL_H), "频率响应", &cfg->do_fr);
    ui_toggle(u, 422, col_row(&c, CTRL_H), "失真 THD+N", &cfg->do_thdn);
    ui_toggle(u, 424, col_row(&c, CTRL_H), "噪声底", &cfg->do_noise);
    ui_toggle(u, 426, col_row(&c, CTRL_H), "上升沿", &cfg->do_step);
    ui_toggle(u, 428, col_row(&c, CTRL_H), "延迟", &cfg->do_delay);
    ui_toggle(u, 430, col_row(&c, CTRL_H), "扣除校准", &cfg->do_cal);

    col_group(&c, "执行");
    if (ui_button(u, 440, col_row(&c, CTRL_H + 2), "开始测试",
                  running ? BTN_NORMAL : BTN_PRIMARY) && !running)
        app_lb_start(a, 0);
    if (ui_button(u, 442, col_row(&c, CTRL_H), "直连校准", BTN_NORMAL) &&
        !running) {
        app_lb_start(a, 1);
    }
    if (running && ui_button(u, 444, col_row(&c, CTRL_H), "中止", BTN_DANGER)) {
        lb_cancel(&a->lb);
        app_set_message(a, "已中止闭环测试");
    }
    if (ui_button(u, 446, col_row(&c, CTRL_H), "打开报告目录", BTN_GHOST))
        a->want_open_dir = 1;
}

static void loop_right(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Col c;
    col_begin(&c, u, r.x + GAP4, r.y + GAP4, r.w - GAP4 * 2);
    Loopback *lb = &a->lb;
    char v[48], v2[48];

    fnum(v, sizeof(v), lb->nfr ? lb->res.max_dev_db : NAN, 2, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "最大偏差", v, "dB",
               lb->nfr ? (fabs(lb->res.max_dev_db) > lb->cfg.tol_db
                              ? g_pal.bad
                              : g_pal.accent)
                       : g_pal.text_faint);
    c.y += 54;
    fnum(v2, sizeof(v2), lb->res.have_thd ? lb->res.thdn_percent : NAN, 3, "");
    big_number(u, as_rect(c.x, c.y, c.w, 46), "THD+N", v2, "%",
               lb->res.have_thd ? g_pal.text : g_pal.text_faint);
    c.y += 54;

    c.y = ui_group(u, c.x, c.y, c.w, "结果") + 4;
    fnum(v, sizeof(v), lb->nfr ? lb->res.flatness_db : NAN, 2, "dB");
    kv(u, col_row(&c, 21), "峰谷差", v, g_pal.text_dim);
    fnum(v, sizeof(v), lb->res.have_delay ? lb->res.delay_ms : NAN, 1, "ms");
    kv(u, col_row(&c, 21), "往返延迟", v, g_pal.text_dim);
    fnum(v, sizeof(v), lb->res.have_noise ? lb->res.noise_dbfs : NAN, 1, "dBFS");
    kv(u, col_row(&c, 21), "噪声底", v, g_pal.text_dim);
    fnum(v, sizeof(v), lb->res.have_step ? lb->res.rise_us : NAN, 1, "µs");
    kv(u, col_row(&c, 21), "上升沿", v, g_pal.text_dim);
    snprintf(v, sizeof(v), "%d", lb->res.clip_count);
    kv(u, col_row(&c, 21), "削波点", v,
       lb->res.clip_count ? g_pal.bad : g_pal.text_dim);
    fnum(v, sizeof(v), lb->res.phase_dev_deg, 0, "°");
    kv(u, col_row(&c, 21), "相位范围", v, g_pal.text_dim);

    c.y = ui_group(u, c.x, c.y, c.w, "状态") + 6;
    ui_bar(u, col_row(&c, 6), lb->progress, g_pal.accent);
    if (lb->state == LB_RUN) {
        char line[96];
        snprintf(line, sizeof(line), "%s · %d%%", lb->stage,
                 (int)(lb->progress * 100));
        ui_text_left(u, col_row(&c, 18), FONT_LABEL, g_pal.accent, line);
    } else if (lb->state == LB_DONE) {
        char v1[64];
        double dev = lb->res.max_dev_db;
        if (fabs(dev) >= 100.0)
            snprintf(v1, sizeof(v1), "最差 —");
        else {
            char d1[32], d2[32];
            fnum(d1, sizeof(d1), dev, 2, "");
            if (lb->res.max_dev_f >= 1000)
                snprintf(d2, sizeof(d2), "%.1f kHz", lb->res.max_dev_f / 1000.0);
            else
                snprintf(d2, sizeof(d2), "%.0f Hz", lb->res.max_dev_f);
            snprintf(v1, sizeof(v1), "%s dB @ %s", d1, d2);
        }
        ui_text_left(u, col_row(&c, 18), FONT_LABEL,
                     lb->rep.verdict ? g_pal.good : g_pal.bad, v1);
        char v2[64];
        snprintf(v2, sizeof(v2), "判据 ±%.2f dB · 削波 %d 点", lb->cfg.tol_db,
                 lb->res.clip_count);
        ui_text_left(u, col_row(&c, 16), FONT_SMALL, g_pal.text_faint, v2);
        ui_text_left(u, col_row(&c, 18), FONT_LABEL,
                     lb->rep.verdict ? g_pal.good : g_pal.bad,
                     lb->rep.verdict ? "判定：通过" : "判定：未通过");
    } else {
        ui_text_left(u, col_row(&c, 18), FONT_LABEL, g_pal.text_faint,
                     a->cal.valid ? "已载入校准，待运行" : "未校准，待运行");
    }
}


/* ------------------------------------------------------------------ */
/* 外壳：页眉 / 导航 / 页脚                                            */
/* ------------------------------------------------------------------ */
static void draw_theme_icon(UI *u, ASRect r, int light) {
    double cx = r.x + r.w * 0.5, cy = r.y + r.h * 0.5;
    if (!light) {
        ui_dot(u, cx, cy, 5.0, g_pal.text_dim);
        ui_dot(u, cx + 2.4, cy - 1.8, 4.4, g_pal.bg);
    } else {
        ui_dot(u, cx, cy, 4.0, g_pal.text_dim);
        for (int i = 0; i < 8; i++) {
            double ang = i * AS_PI / 4.0;
            ui_line(u, cx + cos(ang) * 6.2, cy + sin(ang) * 6.2,
                    cx + cos(ang) * 8.0, cy + sin(ang) * 8.0, g_pal.text_dim, 1);
        }
    }
}

static void draw_header(App *a, UI *u, double w) {
    ui_text(u, GAP6, 14, FONT_LABEL, g_pal.text, "AUDIOSCOPE");
    ui_text(u, GAP6, 36, FONT_MONO, g_pal.text_faint,
            "LOCAL INSTRUMENT  ·  1.1");

    double x = w - GAP6;
    ASRect tb = {x - 28, 18, 28, 28};
    if (ui_button(u, 900, tb, "", BTN_GHOST)) app_theme_toggle(a);
    draw_theme_icon(u, tb, theme_is_light());
    x -= 36;
    ASRect ab = {x - 28, 18, 28, 28};
    if (ui_button(u, 901, ab, "?", BTN_GHOST))
        app_set_message(a, "AUTO F6 · 空格冻结 · T 主题 · ←/→ 时基");
    x -= 40;
    ui_dot(u, x - 8, 32, 2.5, a->running ? g_pal.accent : g_pal.text_faint);
    ui_text_right(u, as_rect(0, 20, x - 16, 24), FONT_MONO, g_pal.text_faint,
                 a->running ? "RUNNING" : "STANDBY");
    ui_line(u, 0, HEADER_H - 1, w, HEADER_H - 1, g_pal.line, 1);
}

static void draw_footer(App *a, UI *u, double w, double h) {
    double y = h - FOOTER_H;
    ui_line(u, 0, y, w, y, g_pal.line, 1);
    char left[256];
    const char *in_name = "—", *out_name = "—";
    for (int i = 0; i < a->ndev; i++) {
        if ((int)a->devs[i].id == a->in_dev) in_name = a->devs[i].name;
        if ((int)a->devs[i].id == a->out_dev) out_name = a->devs[i].name;
    }
    snprintf(left, sizeof(left), "IN %s   OUT %s   %.0f Hz · %d 帧 · 溢出 %d",
             in_name, out_name, a->fs, a->eng.block, (int)a->eng.xruns);
    ui_text_left(u, as_rect(GAP6, y, w * 0.66, FOOTER_H), FONT_MONO,
                 g_pal.text_faint, left);
    ui_text_right(u, as_rect(w * 0.4, y, w * 0.6 - GAP6, FOOTER_H), FONT_MONO,
                  g_pal.text_dim, a->msg);
}

static void loop_advice(App *a, UI *u, ASRect r) {
    panel_card(u, r);
    Loopback *lb = &a->lb;
    micro(u, r.x + GAP4, r.y + GAP2, "结论与调整建议", g_pal.text_faint);
    double y = r.y + 26;
    if (lb->state == LB_IDLE && lb->rep.nadvice == 0) {
        ui_text_left(u, as_rect(r.x + GAP4, y, r.w - GAP4 * 2, 18), FONT_LABEL,
                     g_pal.text_dim,
                     "先做直连校准，再接入被测电路；参数会沿用上一轮。");
        return;
    }
    if (lb->state == LB_RUN) {
        char line[160];
        snprintf(line, sizeof(line), "%s · %d%%", lb->stage,
                 (int)(lb->progress * 100));
        ui_text_left(u, as_rect(r.x + GAP4, y, r.w - GAP4 * 2, 18), FONT_VALUE,
                     g_pal.accent, line);
        return;
    }
    for (int i = 0; i < lb->rep.nadvice && i < 3; i++) {
        ui_text_left(u, as_rect(r.x + GAP4, y, r.w - GAP4 * 2, 18), FONT_LABEL,
                     i == 0 ? g_pal.text : g_pal.text_dim, lb->rep.advice[i]);
        y += 19;
    }
}

static void draw_waterfall(App *a, UI *u, ASRect r) {
    Waterfall *wf = &a->wf;
    mat_recessed(u, r, RADIUS_CARD);
    if (!wf->rgba || wf->count <= 0) return;
    ASRect in = as_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmp = CGBitmapContextCreate(
        wf->rgba, WF_COLS, WF_ROWS, 8, WF_COLS * 4, cs,
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    if (bmp) {
        CGImageRef img = CGBitmapContextCreateImage(bmp);
        if (img) {
            CGContextSaveGState(u->ctx);
            CGContextClipToRect(u->ctx, CGRectMake(in.x, in.y, in.w, in.h));
            CGContextDrawImage(u->ctx, CGRectMake(in.x, in.y, in.w, in.h), img);
            CGContextRestoreGState(u->ctx);
            CGImageRelease(img);
        }
        CGContextRelease(bmp);
    }
    CGColorSpaceRelease(cs);
    ui_stroke_round(u, r, RADIUS_CARD, g_pal.line, 1);
}


/* ------------------------------------------------------------------ */
/* 主视图                                                              */
/* ------------------------------------------------------------------ */
static void draw_main(App *a, UI *u, double w, double h) {
    ui_fill(u, as_rect(0, 0, w, h), g_pal.bg);
    draw_header(a, u, w);

    /* 导航 */
    ASRect nav = {GAP6, HEADER_H, w - GAP6 * 2, NAV_H};
    int tab = a->tab;
    if (ui_navbar(u, 950, nav, as_tab_items(), as_tab_count(), &tab))
        a->tab = tab;
    if (a->tab >= as_tab_count()) a->tab = 0;

    /* 内容区 */
    double top = HEADER_H + NAV_H + GAP4;
    double bottom = h - FOOTER_H - GAP4;
    double ch = bottom - top;
    ASRect left = {GAP6, top, COL_LEFT, ch};
    ASRect right = {w - GAP6 - COL_RIGHT, top, COL_RIGHT, ch};
    ASRect mid = {GAP6 + COL_LEFT + GAP4, top,
                  w - 2 * GAP6 - COL_LEFT - COL_RIGHT - GAP4 * 2, ch};

    switch (a->tab) {
    case TAB_SCOPE:
        scope_chart(a, u, mid);
        scope_left(a, u, left);
        scope_right(a, u, right);
        break;
    case TAB_FFT: {
        ASRect spec = mid;
        spec.h = ch - 150;
        fft_chart(a, u, spec);
        ASRect wfr = {mid.x, mid.y + spec.h + GAP2, mid.w, ch - spec.h - GAP2};
        draw_waterfall(a, u, wfr);
        fft_left(a, u, left);
        fft_right(a, u, right);
        break;
    }
    case TAB_BODE:
        bode_chart(a, u, mid);
        bode_left(a, u, left);
        bode_right(a, u, right);
        break;
    case TAB_LOOP: {
        ASRect chart = mid;
        chart.h = ch - 132;
        loop_chart(a, u, chart);
        ASRect adv = {mid.x, mid.y + chart.h + GAP2, mid.w, ch - chart.h - GAP2};
        loop_advice(a, u, adv);
        loop_left(a, u, left);
        loop_right(a, u, right);
        break;
    }
    default:
        break;
    }
    draw_footer(a, u, w, h);
}

/* 源设置窗口：保留 1.0 的功能，换成同一套语言 */
static void draw_source(App *a, UI *u, double w, double h) {
    ui_fill(u, as_rect(0, 0, w, h), g_pal.bg);
    ui_text(u, GAP6, 16, FONT_LABEL, g_pal.text, "信号源");
    ui_text_right(u, as_rect(0, 20, w - GAP6, 20), FONT_MONO, g_pal.text_faint,
                  "GENERATOR");
    ui_line(u, GAP6, 48, w - GAP6, 48, g_pal.line, 1);

    Col c;
    col_begin(&c, u, GAP6, 60, w - GAP6 * 2);
    GenCfg *g = &a->gen;

    static const char *WV[] = {"正弦", "方波", "三角", "锯齿", "白噪",
                               "粉噪", "双音", "扫频"};
    int wv = AS_MIN(g->wave, 7);
    col_label(&c, "波形");
    if (ui_cycle(u, 800, col_row(&c, CTRL_H), WV, 8, &wv)) {
        g->wave = wv;
        app_sync_gen(a);
    }
    col_label(&c, "频率");
    ui_field(u, 802, col_row(&c, CTRL_H), &g->freq, 0.1, 96000.0, "Hz", 2);
    col_label(&c, "电平");
    ui_field(u, 804, col_row(&c, CTRL_H), &g->amp_db, -80.0, 0.0, "dBFS", 1);
    col_label(&c, "双音频率");
    ui_field(u, 806, col_row(&c, CTRL_H), &g->dual_freq, 1.0, 96000.0, "Hz", 2);
    col_label(&c, "扫频起止");
    ASRect s1 = col_pair_left(&c, CTRL_H, GAP2);
    ASRect s2 = col_pair_right(&c, CTRL_H, GAP2);
    ui_field(u, 808, s1, &g->sweep_f1, 1.0, 96000.0, "Hz", 0);
    ui_field(u, 810, s2, &g->sweep_f2, 1.0, 96000.0, "Hz", 0);
    col_label(&c, "扫频时长");
    ui_field(u, 812, col_row(&c, CTRL_H), &g->sweep_sec, 0.01, 60.0, "s", 2);
    int en = g->enabled;
    if (ui_toggle(u, 814, col_row(&c, CTRL_H), "启用输出", &en)) {
        g->enabled = en;
        app_sync_gen(a);
    }
}

int app_draw_ui(App *a, CGContextRef ctx, double w, double h, int window) {
    UI *u = (window == WIN_SOURCE) ? &a->ui_src : &a->ui;
    ui_begin(u, ctx, w, h, a->now);
    if (window == WIN_SOURCE) draw_source(a, u, w, h);
    else draw_main(a, u, w, h);
    int changed = u->value_changed || u->commit_id >= 0;
    ui_end(u);
    return changed;
}

void app_draw(App *a, CGContextRef ctx, double w, double h, int window) {
    app_draw_ui(a, ctx, w, h, window);
}


