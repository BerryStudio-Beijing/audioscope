/* AudioScope by BerryStudio 0.9 — immediate-mode widgets (CoreGraphics + CoreText). */
#ifndef AS_UI_H
#define AS_UI_H

#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ui/native_theme.h"

typedef struct { double x, y, w, h; } ASRect;

static inline ASRect as_rect(double x, double y, double w, double h) {
    ASRect r = {x, y, w, h};
    return r;
}
static inline int as_hit(ASRect r, double x, double y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

/* 排印角色：Brand 17 / Label 14 / Value 16 / Critical 26 / Mono 12 / Small 11
 *
 * 一个小改动、但不是装饰：Label 与 Small 在 ASCII 文本上带 0.02–0.06em 字距，
 * CJK 文本不加宽（见 BERRY-UI-SPEC §3.3）。 */
enum { FONT_LABEL = 0, FONT_VALUE, FONT_CRIT, FONT_MONO, FONT_SMALL, FONT_BRAND,
       FONT_KIND_COUNT };

typedef enum {
    KEY_NONE = 0, KEY_ENTER, KEY_BACKSPACE, KEY_ESC, KEY_LEFT, KEY_RIGHT,
    KEY_TAB, KEY_UP, KEY_DOWN
} ASKey;

typedef struct {
    CGContextRef ctx;
    double width, height;
    double t;                    /* 秒，用于过渡动画 */

    double mx, my, px, py;
    int mouse_down;
    int press_pending, release_pending;
    int active_id, hot_id, clicked_id;

    int focus_id;
    int focus_visible;           /* 仅键盘操作时画焦点环 */
    char edit[40];
    int edit_len, caret;
    double *edit_target;
    double edit_lo, edit_hi;
    int key, key_char;
    int value_changed, commit_id;

    uint64_t frame;

    /* 导航指示线：时间驱动的缓动状态，按实例保存（不用函数内 static）。
     * 位置单位是设计坐标，t0 是开始时间（秒）。 */
    double nav_x, nav_w;
    double nav_from_x, nav_to_x, nav_from_w, nav_to_w;
    double nav_t0;
    int nav_animating, nav_inited;
} UI;

/* 颜色工具（视图层也会用） */
uint32_t as_alpha(uint32_t col, double a);
uint32_t as_mix(uint32_t a, uint32_t b, double t);

void ui_begin(UI *u, CGContextRef ctx, double w, double h, double t);
void ui_end(UI *u);

void ui_mouse_move(UI *u, double x, double y);
void ui_mouse_down(UI *u, double x, double y);
void ui_mouse_up(UI *u, double x, double y);
void ui_key(UI *u, ASKey k);
void ui_char(UI *u, int codepoint);

/* ---- 基础绘制（材质、发丝线、文字） ---------------------------------- */
void ui_fill(UI *u, ASRect r, uint32_t col);
void ui_fill_round(UI *u, ASRect r, double radius, uint32_t col);
void ui_stroke(UI *u, ASRect r, uint32_t col, double width);
void ui_stroke_round(UI *u, ASRect r, double radius, uint32_t col, double w);
void ui_line(UI *u, double x1, double y1, double x2, double y2, uint32_t col,
             double w);
void ui_line_dash(UI *u, double x1, double y1, double x2, double y2,
                  uint32_t col, double w, const double *pattern, int n);

void ui_text(UI *u, double x, double y_top, int font, uint32_t col,
             const char *txt);
/* 基线对齐的文本：同一行里混排 Label 与 Value 时用它，避免各角色行高不同
 * 造成的上下错位。 */
void ui_text_baseline(UI *u, double x, double baseline, int font, uint32_t col,
                      const char *txt);
void ui_text_baseline_right(UI *u, double right, double baseline, int font,
                            uint32_t col, const char *txt);
double ui_font_ascent(int font);
void ui_text_size(UI *u, int font, const char *txt, double *w, double *h);
double ui_text_width(UI *u, int font, const char *txt);
double ui_font_height(int font);
void ui_text_left(UI *u, ASRect r, int font, uint32_t col, const char *txt);
void ui_text_center(UI *u, ASRect r, int font, uint32_t col, const char *txt);
void ui_text_right(UI *u, ASRect r, int font, uint32_t col, const char *txt);
void ui_text_ellipsis(UI *u, ASRect r, int font, uint32_t col,
                      const char *txt);

/* 材质：raised（卡片/按钮）、recessed（输入框/表格槽） */
void mat_raised(UI *u, ASRect r, double radius);
void mat_recessed(UI *u, ASRect r, double radius);

/* 图表测量面：始终是深石墨面（明色主题也不例外，见 BERRY-UI-SPEC §6.7）。
 * 不要用 mat_recessed 画图表，否则浅色模式会把示波器底色变亮。 */
void ui_plot(UI *u, ASRect r, double radius);

/* 折线：一次建路径、一次描边。之前每个点都单独 stroke，720 点的波形就是
 * 720 次状态切换，是掉帧的主因。 */
void ui_polyline_f(UI *u, double x0, double dx, const float *v, int n,
                   double y_mid, double y_scale, uint32_t col, double w);
void ui_polyline_d(UI *u, double x0, double dx, const double *v, int n,
                   double y_mid, double y_scale, uint32_t col, double w);
void ui_path_xy_f(UI *u, const float *xs, const float *ys, int n, double cx,
                  double cy, double sx, double sy, uint32_t col, double w);

/* 分组标题：小号 uppercase + 发丝线，返回内容起始 y */
double ui_group(UI *u, double x, double y, double w, const char *title);

/* 状态点 / 进度条 */
void ui_dot(UI *u, double cx, double cy, double r, uint32_t col);
void ui_bar(UI *u, ASRect r, double frac, uint32_t col);

/* ---- 控件 ------------------------------------------------------------- */
enum { BTN_NORMAL = 0, BTN_PRIMARY, BTN_DANGER, BTN_GHOST, BTN_SMALL };

int ui_button(UI *u, int id, ASRect r, const char *label, int style);
int ui_toggle(UI *u, int id, ASRect r, const char *label, int *value);
int ui_cycle(UI *u, int id, ASRect r, const char *const *items, int n,
             int *value);
int ui_slider(UI *u, int id, ASRect r, double *value, double lo, double hi);
int ui_field(UI *u, int id, ASRect r, double *value, double lo, double hi,
             const char *unit, int decimals);
int ui_spin(UI *u, int id, ASRect r, double *value, double lo, double hi,
            double step, const char *unit, int decimals);
int ui_navbar(UI *u, int id_base, ASRect r, const char *const *labels, int n,
              int *index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif