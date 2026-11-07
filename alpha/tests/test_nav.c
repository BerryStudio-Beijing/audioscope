/* Regression test: navigation indicator animation.
 *
 * 0.9 的问题：指示线用"每帧向目标靠近 32%"的写法，加上 30 fps 的定时器，
 * 换页时会从上一页慢慢滑过来（看起来永远从左边飞过来），并且帧率一变
 * 动画时长就变。这里锁住三件事：
 *   1. 换页时从当前位置出发，而不是从 x=0 出发；
 *   2. 缓动按时间收敛（MOTION_FAST 之后精确落位）；
 *   3. reduce-motion 时直接落位、不产生动画。
 */
#include "ui/native_ui.h"

#include <CoreGraphics/CoreGraphics.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *what) {
    if (cond) {
        printf("  ok    %s\n", what);
    } else {
        printf("  fail  %s\n", what);
        ++g_fail;
    }
}

static const char *kLabels[7] = {"波形", "频谱", "相位", "测量",
                                 "维修", "报告", "校准"};

int main(void) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        NULL, 1120, 700, 8, 1120 * 4, cs,
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!ctx) {
        printf("  fail  bitmap context\n");
        return 1;
    }

    theme_set(THEME_DARK);
    g_reduce_motion = 0;

    UI u;
    memset(&u, 0, sizeof(u));
    int page = 0;
    const ASRect nav = as_rect(16, 64, 1088, 48);

    ui_begin(&u, ctx, 1120.0, 700.0, 0.0);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    const double x0 = u.nav_x;
    check(u.nav_w > 1.0, "first frame has a visible indicator");
    check(!u.nav_animating, "first frame does not animate");

    /* 跳到第 5 个页签：同一时刻再画一帧，只应该"开始动画"，位置不动。 */
    page = 4;
    ui_begin(&u, ctx, 1120.0, 700.0, 0.0);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    check(u.nav_animating == 1, "page change starts the animation");
    check(fabs(u.nav_x - x0) < 0.001,
          "animation starts from the current position (not from the left)");

    const double target = u.nav_to_x;
    check(target > x0 + 100.0, "target is to the right of the old position");

    /* 半程：必须落在起点与目标之间（ease-out，不会先往回跳）。 */
    ui_begin(&u, ctx, 1120.0, 700.0, MOTION_FAST * 0.5);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    check(u.nav_x > x0 + 1.0 && u.nav_x < target,
          "mid-animation position is between start and target");
    check(u.nav_x - x0 > (target - x0) * 0.5,
          "ease-out is more than half-way at half time");

    /* 时间到：精确落位并结束动画。 */
    ui_begin(&u, ctx, 1120.0, 700.0, MOTION_FAST + 0.001);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    check(fabs(u.nav_x - target) < 0.001, "settles exactly on the target");
    check(!u.nav_animating, "animation completes after MOTION_FAST");

    /* 低帧率（每帧 200 ms）也只花 MOTION_FAST 的时间。 */
    page = 6;
    ui_begin(&u, ctx, 1120.0, 700.0, 1.0);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    const double t1 = 1.0;
    check(u.nav_animating == 1, "slow frame still starts an animation");
    ui_begin(&u, ctx, 1120.0, 700.0, t1 + MOTION_FAST + 0.001);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    check(!u.nav_animating, "animation length is time-based, not frame-based");

    /* reduce-motion：直接落位。 */
    g_reduce_motion = 1;
    page = 0;
    ui_begin(&u, ctx, 1120.0, 700.0, 2.0);
    ui_navbar(&u, 100, nav, kLabels, 7, &page);
    ui_end(&u);
    check(!u.nav_animating && fabs(u.nav_x - u.nav_to_x) < 0.001,
          "reduce-motion snaps without animating");

    CGContextRelease(ctx);
    printf("%d failure(s)\n", g_fail);
    return g_fail ? 1 : 0;
}
