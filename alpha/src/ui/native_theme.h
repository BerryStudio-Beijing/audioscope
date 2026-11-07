/* AudioScope by BerryStudio 0.9 — design tokens (Precision Instrument).
 *
 * 语义 token 只在这里定义；组件里不要再写十六进制颜色。
 * 暗色是主用主题，明色不是反相。
 */
#ifndef AS_THEME_H
#define AS_THEME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { THEME_DARK = 0, THEME_LIGHT = 1 };

typedef struct {
    uint32_t bg, card, card_alt, raised;
    uint32_t line, line_soft, divider;
    uint32_t text, text_dim, text_faint;
    uint32_t accent, accent_soft;
    uint32_t cyan, good, info, warn, bad, violet;
    uint32_t good_ink, warn_ink, bad_ink, cyan_ink;
    uint32_t chart_bg, chart_grid, chart_axis, curve_main, curve_hold, curve_fill;
    uint32_t curve_alt, curve_math, curve_warn;
    uint32_t overlay;
} Palette;

extern Palette g_pal;
extern int g_theme_mode;      /* THEME_DARK / THEME_LIGHT */
extern int g_reduce_motion;   /* 1 = 关闭过渡动画 */

void theme_set(int mode);
void theme_toggle(void);
int  theme_is_light(void);

/* 4px 基础网格：组内 8，区块 16，大分组 24，图表安全区 32 */
#define GAP1 4
#define GAP2 8
#define GAP4 16
#define GAP6 24
#define GAP8 32

#define RADIUS_CARD 6.0
#define RADIUS_CTRL 5.0

#define ROW_H 32.0
#define CTRL_H 28.0
#define HEADER_H 64.0
#define NAV_H 48.0
#define FOOTER_H 32.0
#define COL_LEFT 232.0
#define COL_RIGHT 236.0

/* 主题切换的淡入时长（秒），0 表示立即 */
#define MOTION_FAST 0.14

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif