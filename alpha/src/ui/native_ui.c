#include "ui/native_ui.h"

#include <CoreText/CoreText.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/native_compat.h"

/* ------------------------------------------------------------------ */
/* 颜色                                                                */
/* ------------------------------------------------------------------ */
static CGColorRef color_of(uint32_t c) {
    CGFloat comp[4];
    comp[0] = ((c >> 24) & 0xff) / 255.0;
    comp[1] = ((c >> 16) & 0xff) / 255.0;
    comp[2] = ((c >> 8) & 0xff) / 255.0;
    comp[3] = (c & 0xff) / 255.0;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGColorRef col = CGColorCreate(cs, comp);
    CGColorSpaceRelease(cs);
    return col;
}

uint32_t as_mix(uint32_t a, uint32_t b, double t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    uint32_t out = 0;
    for (int sh = 0; sh <= 24; sh += 8) {
        double ca = (a >> sh) & 0xff, cb = (b >> sh) & 0xff;
        out |= (uint32_t)((ca + (cb - ca) * t) + 0.5) << sh;
    }
    return out;
}

uint32_t as_alpha(uint32_t c, double a) {
    uint32_t al = (uint32_t)(AS_CLAMP(a, 0.0, 1.0) * 255.0 + 0.5);
    return (c & 0xffffff00u) | al;
}

static uint32_t brighten(uint32_t c, double amount) {
    if (amount >= 0) return as_mix(c, 0xffffffff, amount);
    return as_mix(c, 0x000000ff, -amount);
}

/* ------------------------------------------------------------------ */
/* 字体：Avenir Next（几何无衬线）+ Menlo（技术信息）                  */
/* ------------------------------------------------------------------ */
typedef struct {
    CTFontRef font;
    double size;
    double ascent, descent, line;
} Face;

static Face g_faces[FONT_KIND_COUNT];
static CFArrayRef g_tnum_feats;

static const char *FACE_NAME[FONT_KIND_COUNT] = {
    "Geist-Regular", "Geist-Medium", "Geist-Bold",
    "GeistMono-Regular", "Geist-Regular"};
static const double FACE_SIZE[FONT_KIND_COUNT] = {14, 16, 26, 12, 11};

static CFArrayRef tnum_features(void) {
    if (g_tnum_feats) return g_tnum_feats;
    int type = 6;    /* kNumberSpacingType */
    int sel = 0;     /* kMonospacedNumbersSelector */
    CFNumberRef ntype = CFNumberCreate(NULL, kCFNumberIntType, &type);
    CFNumberRef nsel = CFNumberCreate(NULL, kCFNumberIntType, &sel);
    const void *keys[2] = {kCTFontFeatureTypeIdentifierKey,
                           kCTFontFeatureSelectorIdentifierKey};
    const void *vals[2] = {ntype, nsel};
    CFDictionaryRef feat = CFDictionaryCreate(
        NULL, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    const void *arr[1] = {feat};
    g_tnum_feats = CFArrayCreate(NULL, arr, 1, &kCFTypeArrayCallBacks);
    CFRelease(feat);
    CFRelease(ntype);
    CFRelease(nsel);
    return g_tnum_feats;
}

static void face_init(int kind) {
    if (g_faces[kind].font) return;
    CFStringRef name =
        CFStringCreateWithCString(NULL, FACE_NAME[kind], kCFStringEncodingUTF8);
    CTFontRef f = CTFontCreateWithName(name, FACE_SIZE[kind], NULL);
    CFRelease(name);
    if (!f) f = CTFontCreateWithName(CFSTR("Helvetica"), FACE_SIZE[kind], NULL);
    Face *fc = &g_faces[kind];
    fc->font = f;
    fc->size = FACE_SIZE[kind];
    fc->ascent = CTFontGetAscent(f);
    fc->descent = CTFontGetDescent(f);
    fc->line = fc->ascent + fc->descent + CTFontGetLeading(f);
}

double ui_font_height(int font) {
    face_init(AS_CLAMP(font, 0, FONT_KIND_COUNT - 1));
    return g_faces[font].line;
}

static CTLineRef make_line(int font, uint32_t col, const char *txt) {
    if (!txt || !*txt) return NULL;
    font = AS_CLAMP(font, 0, FONT_KIND_COUNT - 1);
    face_init(font);
    CFStringRef s = CFStringCreateWithCString(NULL, txt, kCFStringEncodingUTF8);
    if (!s) return NULL;
    CGColorRef c = color_of(col);

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
        NULL, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kCTFontAttributeName, g_faces[font].font);
    CFDictionarySetValue(attrs, kCTForegroundColorAttributeName, c);
    if (font == FONT_VALUE || font == FONT_CRIT)
        CFDictionarySetValue(attrs, kCTFontFeatureSettingsAttribute,
                             tnum_features());
    if (font == FONT_LABEL || font == FONT_MONO) {
        double kern = (font == FONT_LABEL) ? 0.45 : 0.2;
        CFNumberRef kn = CFNumberCreate(NULL, kCFNumberDoubleType, &kern);
        CFDictionarySetValue(attrs, kCTKernAttributeName, kn);
        CFRelease(kn);
    }
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    CFRelease(as);
    CFRelease(attrs);
    CGColorRelease(c);
    CFRelease(s);
    return line;
}

double ui_text_width(UI *u, int font, const char *txt) {
    (void)u;
    CTLineRef line = make_line(font, g_pal.text, txt);
    double w = line ? CTLineGetTypographicBounds(line, NULL, NULL, NULL) : 0;
    if (line) CFRelease(line);
    return w;
}

void ui_text_size(UI *u, int font, const char *txt, double *w, double *h) {
    if (w) *w = ui_text_width(u, font, txt);
    if (h) *h = ui_font_height(font);
}

void ui_text(UI *u, double x, double y_top, int font, uint32_t col,
             const char *txt) {
    CTLineRef line = make_line(font, col, txt);
    if (!line) return;
    face_init(font);
    double asc = g_faces[font].ascent;
    CGContextRef ctx = u->ctx;
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, x, y_top + asc);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);
    CFRelease(line);
}

static void text_aligned(UI *u, ASRect r, int font, uint32_t col,
                         const char *txt, int align) {
    if (!txt || !*txt) return;
    font = AS_CLAMP(font, 0, FONT_KIND_COUNT - 1);
    face_init(font);
    /* 先按宽度截断，避免压到相邻列 */
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", txt);
    CTLineRef line = make_line(font, col, buf);
    if (!line) return;
    double width = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    if (width > r.w && r.w > 16) {
        size_t len = strlen(buf);
        while (len > 1 && width > r.w) {
            len--;
            while (len > 0 && ((unsigned char)buf[len] & 0xc0) == 0x80) len--;
            buf[len] = 0;
            CFRelease(line);
            line = make_line(font, col, buf);
            width = line ? CTLineGetTypographicBounds(line, NULL, NULL, NULL) : 0;
        }
        if (width > r.w && len > 2) {          /* 加省略号 */
            buf[len - 1] = 0;
            strncat(buf, "\xe2\x80\xa6", sizeof(buf) - strlen(buf) - 1);
            if (line) CFRelease(line);
            line = make_line(font, col, buf);
            width = line ? CTLineGetTypographicBounds(line, NULL, NULL, NULL) : 0;
        }
        if (!line) return;
    }
    double asc = g_faces[font].ascent;
    double lh = g_faces[font].line;
    double px = r.x;
    if (align == 1) px = r.x + r.w - width;
    else if (align == 2) px = r.x + (r.w - width) * 0.5;
    double py = r.y + (r.h - lh) * 0.5 + asc;
    CGContextRef ctx = u->ctx;
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, px, py);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);
    CFRelease(line);
}

void ui_text_left(UI *u, ASRect r, int font, uint32_t col, const char *txt) {
    text_aligned(u, r, font, col, txt, 0);
}
void ui_text_center(UI *u, ASRect r, int font, uint32_t col, const char *txt) {
    text_aligned(u, r, font, col, txt, 2);
}
void ui_text_right(UI *u, ASRect r, int font, uint32_t col, const char *txt) {
    text_aligned(u, r, font, col, txt, 1);
}
void ui_text_ellipsis(UI *u, ASRect r, int font, uint32_t col,
                      const char *txt) {
    text_aligned(u, r, font, col, txt, 0);
}

/* ------------------------------------------------------------------ */
/* 绘制原语                                                            */
/* ------------------------------------------------------------------ */
void ui_fill(UI *u, ASRect r, uint32_t col) {
    CGColorRef c = color_of(col);
    CGContextSetFillColorWithColor(u->ctx, c);
    CGContextFillRect(u->ctx, CGRectMake(r.x, r.y, r.w, r.h));
    CGColorRelease(c);
}

void ui_fill_round(UI *u, ASRect r, double radius, uint32_t col) {
    if (r.w <= 0 || r.h <= 0) return;
    CGColorRef c = color_of(col);
    CGPathRef p = CGPathCreateWithRoundedRect(
        CGRectMake(r.x, r.y, r.w, r.h), radius, radius, NULL);
    CGContextAddPath(u->ctx, p);
    CGContextSetFillColorWithColor(u->ctx, c);
    CGContextFillPath(u->ctx);
    CGPathRelease(p);
    CGColorRelease(c);
}

void ui_stroke(UI *u, ASRect r, uint32_t col, double width) {
    CGColorRef c = color_of(col);
    CGContextSetStrokeColorWithColor(u->ctx, c);
    CGContextSetLineWidth(u->ctx, width);
    CGContextStrokeRect(u->ctx,
                        CGRectMake(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1));
    CGColorRelease(c);
}

void ui_stroke_round(UI *u, ASRect r, double radius, uint32_t col, double w) {
    if (r.w <= 1 || r.h <= 1) return;
    CGColorRef c = color_of(col);
    CGPathRef p = CGPathCreateWithRoundedRect(
        CGRectMake(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1), radius, radius,
        NULL);
    CGContextAddPath(u->ctx, p);
    CGContextSetStrokeColorWithColor(u->ctx, c);
    CGContextSetLineWidth(u->ctx, w);
    CGContextStrokePath(u->ctx);
    CGPathRelease(p);
    CGColorRelease(c);
}

void ui_line(UI *u, double x1, double y1, double x2, double y2, uint32_t col,
             double w) {
    CGContextSetLineDash(u->ctx, 0, NULL, 0);
    CGColorRef c = color_of(col);
    CGContextSetStrokeColorWithColor(u->ctx, c);
    CGContextSetLineWidth(u->ctx, w);
    CGContextMoveToPoint(u->ctx, x1, y1);
    CGContextAddLineToPoint(u->ctx, x2, y2);
    CGContextStrokePath(u->ctx);
    CGColorRelease(c);
}

void ui_line_dash(UI *u, double x1, double y1, double x2, double y2,
                  uint32_t col, double w, const double *pattern, int n) {
    CGColorRef c = color_of(col);
    CGContextSetStrokeColorWithColor(u->ctx, c);
    CGContextSetLineWidth(u->ctx, w);
    CGContextSetLineDash(u->ctx, 0, pattern, (size_t)n);
    CGContextMoveToPoint(u->ctx, x1, y1);
    CGContextAddLineToPoint(u->ctx, x2, y2);
    CGContextStrokePath(u->ctx);
    CGContextSetLineDash(u->ctx, 0, NULL, 0);
    CGColorRelease(c);
}


/* ------------------------------------------------------------------ */
/* 材质：单一光源在左上                                                */
/* ------------------------------------------------------------------ */
void mat_raised(UI *u, ASRect r, double radius) {
    /* 底色 + 左上提亮 / 右下压暗的宽渐变 + 底部接触阴影 */
    ui_fill_round(u, r, radius, g_pal.card_alt);
    CGContextRef ctx = u->ctx;
    CGPathRef clip = CGPathCreateWithRoundedRect(
        CGRectMake(r.x, r.y, r.w, r.h), radius, radius, NULL);
    CGContextSaveGState(ctx);
    CGContextAddPath(ctx, clip);
    CGContextClip(ctx);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    double hi = theme_is_light() ? 0.30 : 0.055;
    double lo = theme_is_light() ? 0.045 : 0.075;
    CGFloat c0[4] = {1, 1, 1, hi};
    CGFloat c1[4] = {1, 1, 1, 0.0};
    CGFloat c2[4] = {0, 0, 0, lo};
    CGColorRef g0 = CGColorCreate(cs, c0);
    CGColorRef g1 = CGColorCreate(cs, c1);
    CGColorRef g2 = CGColorCreate(cs, c2);
    CGFloat loc[3] = {0.0, 0.45, 1.0};
    const void *cols[3] = {g0, g1, g2};
    CFArrayRef arr = CFArrayCreate(NULL, cols, 3, &kCFTypeArrayCallBacks);
    CGGradientRef grad = CGGradientCreateWithColors(cs, arr, loc);
    CGContextDrawLinearGradient(ctx, grad, CGPointMake(r.x, r.y),
                                CGPointMake(r.x + r.w, r.y + r.h),
                                kCGGradientDrawsBeforeStartLocation |
                                    kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(grad);
    CFRelease(arr);
    CGColorRelease(g0);
    CGColorRelease(g1);
    CGColorRelease(g2);
    CGColorSpaceRelease(cs);
    CGContextRestoreGState(ctx);
    CGPathRelease(clip);
    /* 顶部高光 + 底边接触阴影 */
    ui_line(u, r.x + radius, r.y + 0.5, r.x + r.w - radius, r.y + 0.5,
            as_alpha(0xffffffff, theme_is_light() ? 0.55 : 0.10), 1);
    ui_line(u, r.x + radius, r.y + r.h - 0.5, r.x + r.w - radius,
            r.y + r.h - 0.5, as_alpha(0x000000ff, 0.22), 1);
    ui_stroke_round(u, r, radius, g_pal.line_soft, 1);
}

void mat_recessed(UI *u, ASRect r, double radius) {
    ui_fill_round(u, r, radius, g_pal.card);
    CGPathRef clip = CGPathCreateWithRoundedRect(
        CGRectMake(r.x, r.y, r.w, r.h), radius, radius, NULL);
    CGContextSaveGState(u->ctx);
    CGContextAddPath(u->ctx, clip);
    CGContextClip(u->ctx);
    /* 顶部内阴影 + 底部细高光 */
    ui_fill(u, as_rect(r.x, r.y, r.w, 1.5),
            as_alpha(0x000000ff, theme_is_light() ? 0.10 : 0.34));
    ui_fill(u, as_rect(r.x, r.y + r.h - 1.0, r.w, 1.0),
            as_alpha(0xffffffff, theme_is_light() ? 0.65 : 0.08));
    CGContextRestoreGState(u->ctx);
    CGPathRelease(clip);
    ui_stroke_round(u, r, radius, g_pal.line, 1);
}

void ui_dot(UI *u, double cx, double cy, double r, uint32_t col) {
    CGColorRef c = color_of(col);
    CGContextSetFillColorWithColor(u->ctx, c);
    CGContextFillEllipseInRect(u->ctx,
                               CGRectMake(cx - r, cy - r, r * 2, r * 2));
    CGColorRelease(c);
}

double ui_group(UI *u, double x, double y, double w, const char *title) {
    ASRect lr = {x, y, w, 16};
    ui_text_left(u, lr, FONT_LABEL, g_pal.text_dim, title);
    ui_line(u, x, y + 20, x + w, y + 20, g_pal.line_soft, 1);
    return y + 28;
}

void ui_bar(UI *u, ASRect r, double frac, uint32_t col) {
    mat_recessed(u, r, r.h * 0.5);
    frac = AS_CLAMP(frac, 0.0, 1.0);
    if (frac > 0.002) {
        ASRect f = {r.x + 1, r.y + 1, (r.w - 2) * frac, r.h - 2};
        ui_fill_round(u, f, (r.h - 2) * 0.5, col);
    }
}

/* ------------------------------------------------------------------ */
/* 输入状态                                                            */
/* ------------------------------------------------------------------ */
void ui_begin(UI *u, CGContextRef ctx, double w, double h, double t) {
    u->ctx = ctx;
    u->width = w;
    u->height = h;
    u->t = t;
    u->clicked_id = -1;
    u->hot_id = -1;
    u->commit_id = -1;
    u->value_changed = 0;
    u->frame++;
}

void ui_end(UI *u) {
    u->press_pending = 0;
    u->release_pending = 0;
    u->key = KEY_NONE;
    u->key_char = 0;
    if (!u->mouse_down) u->active_id = -1;
}

void ui_mouse_move(UI *u, double x, double y) {
    u->px = u->mx;
    u->py = u->my;
    u->mx = x;
    u->my = y;
}

void ui_mouse_down(UI *u, double x, double y) {
    u->mx = u->px = x;
    u->my = u->py = y;
    u->mouse_down = 1;
    u->press_pending = 1;
    u->focus_visible = 0;
}

void ui_mouse_up(UI *u, double x, double y) {
    u->mx = x;
    u->my = y;
    u->mouse_down = 0;
    u->release_pending = 1;
}

void ui_key(UI *u, ASKey k) {
    u->key = k;
    u->focus_visible = 1;
}

void ui_char(UI *u, int cp) {
    u->key_char = cp;
    u->focus_visible = 1;
}


/* ------------------------------------------------------------------ */
/* 控件内部                                                            */
/* ------------------------------------------------------------------ */
static void commit_edit(UI *u) {
    if (u->focus_id && u->edit_target) {
        char *end = NULL;
        double v = strtod(u->edit, &end);
        if (end != u->edit) {
            v = AS_CLAMP(v, u->edit_lo, u->edit_hi);
            if (fabs(v - *u->edit_target) > 1e-12) {
                *u->edit_target = v;
                u->value_changed = 1;
            }
            u->commit_id = u->focus_id;
        }
    }
    u->focus_id = 0;
    u->edit_target = NULL;
}

static int widget_state(UI *u, int id, ASRect r, int *hover, int *pressed) {
    int inside = as_hit(r, u->mx, u->my);
    if (inside) u->hot_id = id;
    if (u->press_pending && inside) {
        if (u->focus_id && u->focus_id != id) commit_edit(u);
        u->active_id = id;
    }
    *hover = inside;
    *pressed = (u->active_id == id && u->mouse_down);
    if (u->release_pending && u->active_id == id && inside) {
        u->clicked_id = id;
        return 1;
    }
    return 0;
}

static void chevron(UI *u, double cx, double cy, double dir, uint32_t col) {
    for (int pass = 0; pass < 2; pass++) {
        double ox = (pass == 0) ? -3.0 : -1.0;
        ui_line(u, cx + ox + dir * 2.0, cy - 4.0, cx + ox, cy, col, 1.0);
        ui_line(u, cx + ox, cy, cx + ox + dir * 2.0, cy + 4.0, col, 1.0);
    }
}

int ui_button(UI *u, int id, ASRect r, const char *label, int style) {
    int hover = 0, pressed = 0;
    int clicked = widget_state(u, id, r, &hover, &pressed);
    uint32_t bg = g_pal.card_alt, fg = g_pal.text, border = g_pal.line;
    int has_bg = 1;
    if (style == BTN_PRIMARY) {
        bg = g_pal.accent_soft;
        fg = g_pal.accent;
        border = as_alpha(g_pal.accent, 0.75);
    } else if (style == BTN_DANGER) {
        bg = g_pal.card_alt;
        fg = g_pal.bad;
        border = as_alpha(g_pal.bad, 0.5);
    } else if (style == BTN_GHOST) {
        has_bg = 0;
        fg = g_pal.text_dim;
        border = 0;
    }
    if (hover && has_bg) bg = brighten(bg, theme_is_light() ? 0.045 : 0.085);
    if (hover && !has_bg)
        ui_fill_round(u, r, RADIUS_CTRL, as_alpha(0xffffffff, 0.05));
    if (pressed && has_bg) bg = brighten(bg, -0.16);
    if (has_bg) {
        ui_fill_round(u, r, RADIUS_CTRL, bg);
        ui_stroke_round(u, r, RADIUS_CTRL, border, 1);
    }
    if (style != BTN_SMALL && u->focus_visible && u->focus_id == u->active_id)
        ui_stroke_round(u, as_rect(r.x + 2, r.y + 2, r.w - 4, r.h - 4),
                        RADIUS_CTRL - 1, as_alpha(g_pal.accent, 0.9), 1);
    ASRect tr = r;
    if (pressed) tr.y += 1.0;
    int font = (style == BTN_SMALL) ? FONT_SMALL : FONT_VALUE;
    ui_text_center(u, tr, font, fg, label);
    return clicked;
}

int ui_toggle(UI *u, int id, ASRect r, const char *label, int *value) {
    int hover = 0, pressed = 0;
    int clicked = widget_state(u, id, r, &hover, &pressed);
    if (clicked) {
        *value = !*value;
        u->value_changed = 1;
    }
    int on = *value != 0;
    /* 选中只提亮一档 + 左侧 accent 竖条 + 1px 下边线，不铺满整块绿色 */
    uint32_t bg = g_pal.card_alt;
    if (on) bg = as_mix(g_pal.card_alt, g_pal.accent_soft, 0.55);
    if (hover) bg = brighten(bg, theme_is_light() ? 0.04 : 0.07);
    if (pressed) bg = brighten(bg, -0.12);
    ui_fill_round(u, r, RADIUS_CTRL, bg);
    ui_stroke_round(u, r, RADIUS_CTRL, on ? as_alpha(g_pal.accent, 0.55)
                                          : g_pal.line, 1);
    if (on) {
        ui_fill_round(u, as_rect(r.x + 1, r.y + 6, 2, r.h - 12), 1,
                      g_pal.accent);
    }
    ASRect tr = r;
    tr.x += on ? 8 : 0;
    tr.w -= on ? 8 : 0;
    ui_text_center(u, tr, FONT_LABEL, on ? g_pal.accent : g_pal.text_faint, label);
    return clicked;
}

int ui_cycle(UI *u, int id, ASRect r, const char *const *items, int n,
             int *value) {
    if (n <= 0) return 0;
    mat_recessed(u, r, RADIUS_CTRL);
    double zone = 22.0;
    int hover = as_hit(r, u->mx, u->my);
    int lh = hover && u->mx < r.x + zone;
    int rh = hover && u->mx > r.x + r.w - zone;
    if (lh) ui_fill(u, as_rect(r.x + 1, r.y + 1, zone, r.h - 2),
                    as_alpha(0xffffffff, 0.05));
    if (rh) ui_fill(u, as_rect(r.x + r.w - zone - 1, r.y + 1, zone, r.h - 2),
                    as_alpha(0xffffffff, 0.05));
    int changed = 0;
    if (ui_button(u, id, as_rect(r.x, r.y, zone, r.h), "", BTN_GHOST)) {
        *value = (*value + n - 1) % n;
        changed = 1;
    }
    if (ui_button(u, id + 1, as_rect(r.x + r.w - zone, r.y, zone, r.h), "",
                  BTN_GHOST)) {
        *value = (*value + 1) % n;
        changed = 1;
    }
    chevron(u, r.x + zone * 0.5, r.y + r.h * 0.5, 1.0, g_pal.text_dim);
    chevron(u, r.x + r.w - zone * 0.5, r.y + r.h * 0.5, -1.0, g_pal.text_dim);
    ASRect mid = {r.x + zone, r.y, r.w - zone * 2, r.h};
    ui_text_center(u, mid, FONT_VALUE, g_pal.text, items[*value]);
    if (changed) u->value_changed = 1;
    return changed;
}


int ui_slider(UI *u, int id, ASRect r, double *value, double lo, double hi) {
    int hover = 0, pressed = 0;
    widget_state(u, id, r, &hover, &pressed);
    int changed = 0;
    if (pressed && u->mouse_down && r.w > 2) {
        double f = AS_CLAMP((u->mx - r.x) / r.w, 0.0, 1.0);
        double v = lo + f * (hi - lo);
        if (fabs(v - *value) > 1e-9) {
            *value = v;
            changed = 1;
            u->value_changed = 1;
        }
    }
    ASRect track = {r.x, r.y + r.h * 0.5 - 2, r.w, 4};
    mat_recessed(u, track, 2);
    double f = (hi > lo) ? AS_CLAMP((*value - lo) / (hi - lo), 0.0, 1.0) : 0;
    if (f > 0.001) {
        ASRect fill = {track.x + 1, track.y + 1, (track.w - 2) * f, track.h - 2};
        ui_fill_round(u, fill, 1.5, g_pal.accent);
    }
    double hx = r.x + r.w * f;
    ASRect knob = {hx - 5, r.y + r.h * 0.5 - 5, 10, 10};
    ui_fill_round(u, knob, 5,
                  pressed ? brighten(g_pal.card_alt, 0.12) : g_pal.card_alt);
    ui_stroke_round(u, knob, 5, pressed ? g_pal.accent : g_pal.line, 1);
    return changed;
}

int ui_field(UI *u, int id, ASRect r, double *value, double lo, double hi,
             const char *unit, int decimals) {
    int hover = 0, pressed = 0;
    int clicked = widget_state(u, id, r, &hover, &pressed);
    if (clicked) {
        u->focus_id = id;
        u->edit_target = value;
        u->edit_lo = lo;
        u->edit_hi = hi;
        snprintf(u->edit, sizeof(u->edit), "%.*f", decimals, *value);
        u->edit_len = (int)strlen(u->edit);
        u->caret = u->edit_len;
    }
    int editing = (u->focus_id == id);
    char shown[64];
    if (editing) {
        snprintf(shown, sizeof(shown), "%s", u->edit);
        if (u->key == KEY_ENTER || u->key == KEY_TAB) {
            commit_edit(u);
            editing = 0;
            snprintf(shown, sizeof(shown), "%.*f", decimals, *value);
        } else if (u->key == KEY_ESC) {
            u->focus_id = 0;
            u->edit_target = NULL;
            editing = 0;
            snprintf(shown, sizeof(shown), "%.*f", decimals, *value);
        } else if (u->key == KEY_BACKSPACE) {
            if (u->edit_len > 0) u->edit[--u->edit_len] = 0;
            snprintf(shown, sizeof(shown), "%s", u->edit);
        } else if (u->key_char) {
            int c = u->key_char;
            int ok = (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
            if (ok && u->edit_len < (int)sizeof(u->edit) - 2) {
                u->edit[u->edit_len++] = (char)c;
                u->edit[u->edit_len] = 0;
                snprintf(shown, sizeof(shown), "%s", u->edit);
            }
        }
    } else {
        snprintf(shown, sizeof(shown), "%.*f", decimals, *value);
    }

    if (editing) ui_fill_round(u, r, RADIUS_CTRL, g_pal.card);
    else mat_recessed(u, r, RADIUS_CTRL);
    if (editing || hover)
        ui_stroke_round(u, r, RADIUS_CTRL, editing ? g_pal.accent : g_pal.line, 1);

    ASRect tf = {r.x + 7, r.y, r.w - 14, r.h};
    if (unit && *unit) {
        double uw = ui_text_width(u, FONT_SMALL, unit);
        ui_text_right(u, (ASRect){r.x + r.w - uw - 7, r.y, uw, r.h}, FONT_SMALL,
                      g_pal.text_faint, unit);
        tf.w = r.w - uw - 16;
    }
    ui_text_right(u, tf, FONT_VALUE, editing ? g_pal.accent : g_pal.text, shown);
    if (editing && (g_reduce_motion || ((u->frame / 16) & 1) == 0)) {
        double tw = ui_text_width(u, FONT_VALUE, shown);
        double cx = tf.x + tf.w - tw - 2;
        ui_line(u, cx, r.y + 5, cx, r.y + r.h - 5, g_pal.accent, 1);
    }
    return clicked;
}


int ui_spin(UI *u, int id, ASRect r, double *value, double lo, double hi,
            double step, const char *unit, int decimals) {
    double bw = 22;
    int changed = 0;
    ASRect minus = {r.x, r.y, bw, r.h};
    ASRect plus = {r.x + r.w - bw, r.y, bw, r.h};
    ASRect fld = {r.x + bw + 3, r.y, r.w - 2 * bw - 6, r.h};

    mat_recessed(u, r, RADIUS_CTRL);
    ui_fill(u, as_rect(r.x + 1, r.y + 1, bw, r.h - 2),
            as_alpha(0xffffffff, 0.03));
    ui_fill(u, as_rect(r.x + r.w - bw - 1, r.y + 1, bw, r.h - 2),
            as_alpha(0xffffffff, 0.03));

    if (ui_button(u, id, minus, "", BTN_GHOST)) {
        *value = AS_MAX(lo, *value - step);
        changed = 1;
    }
    if (ui_button(u, id + 1, plus, "", BTN_GHOST)) {
        *value = AS_MIN(hi, *value + step);
        changed = 1;
    }
    double cy = r.y + r.h * 0.5;
    ui_line(u, r.x + bw * 0.5 - 4, cy, r.x + bw * 0.5 + 4, cy, g_pal.text_dim, 1);
    ui_line(u, r.x + r.w - bw * 0.5 - 4, cy, r.x + r.w - bw * 0.5 + 4, cy,
            g_pal.text_dim, 1);
    ui_line(u, r.x + r.w - bw * 0.5, cy - 4, r.x + r.w - bw * 0.5, cy + 4,
            g_pal.text_dim, 1);
    ui_field(u, id + 2, fld, value, lo, hi, unit, decimals);
    if (changed) u->value_changed = 1;
    return changed;
}

int ui_navbar(UI *u, int id_base, ASRect r, const char *const *labels, int n,
              int *index) {
    static double ind_x = -1, ind_w = 0;
    int changed = 0;
    double x = r.x;
    double target_x = r.x, target_w = 0;
    for (int i = 0; i < n; i++) {
        double tw = ui_text_width(u, FONT_LABEL, labels[i]);
        double w = tw + 32;
        ASRect t = {x, r.y, w, r.h};
        int hover = 0, pressed = 0;
        int clicked = widget_state(u, id_base + i, t, &hover, &pressed);
        int sel = (*index == i);
        if (hover && !sel)
            ui_fill_round(u, as_rect(t.x + 4, t.y + 10, t.w - 8, t.h - 20), 4,
                          as_alpha(0xffffffff, 0.04));
        ui_text_center(u, t, FONT_LABEL,
                       sel ? g_pal.accent
                           : (hover ? g_pal.text : g_pal.text_faint),
                       labels[i]);
        if (sel) {
            target_x = t.x + t.w * 0.15;
            target_w = t.w * 0.7;
        }
        if (clicked && *index != i) {
            *index = i;
            changed = 1;
            u->value_changed = 1;
        }
        x += w + 4;
    }
    if (ind_x < 0 || g_reduce_motion || target_w <= 0) {
        ind_x = target_x;
        ind_w = target_w;
    } else {
        ind_x += (target_x - ind_x) * 0.32;
        ind_w += (target_w - ind_w) * 0.32;
    }
    if (ind_w > 0)
        ui_fill_round(u, as_rect(ind_x, r.y + r.h - 2, ind_w, 2), 1,
                      g_pal.accent);
    ui_line(u, r.x, r.y + r.h - 1, r.x + r.w, r.y + r.h - 1, g_pal.line_soft, 1);
    return changed;
}

