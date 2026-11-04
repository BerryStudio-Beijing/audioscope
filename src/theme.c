#include "theme.h"

int g_theme_mode = THEME_DARK;
int g_reduce_motion = 0;

/* 暗色：接近黑但带绿灰，文字是低饱和暖灰绿 */
static const Palette DARK = {
    /* bg        card      cardAlt   raised  */
    0x171a1bff, 0x202425ff, 0x2a2f30ff, 0x2a2f30ff,
    /* line      lineSoft  divider */
    0x3b4242ff, 0x303637ff, 0x303637ff,
    /* text      textDim   textFaint */
    0x969c98ff, 0x909994ff, 0x858f89ff,
    /* accent    accentSoft */
    0x819e90ff, 0x30463eff,
    /* cyan      good      info      warn      bad       violet */
    0x80989fff, 0x819e90ff, 0x819e90ff, 0xb19870ff, 0xc08278ff, 0x94949cff,
    /* inks（暗色下同主色） */
    0x819e90ff, 0xb19870ff, 0xc08278ff, 0x80989fff,
    /* chartBg   grid      axis      curveMain  curveHold  fill */
    0x202425ff, 0x303637ff, 0x858f89ff, 0xd6dad6ff, 0x858f89ff, 0x819e90ff,
    0x93aeb5ff, 0xa39fc4ff, 0xb19870ff, 0x0000008c,
};

/* 明色：暖灰绿纸面；图表用深石墨测量面 */
static const Palette LIGHT = {
    0xe5e6dfff, 0xdce0d9ff, 0xe9ece5ff, 0xdce0d9ff,
    0xc4cdc5ff, 0xd0d7cfff, 0xd0d7cfff,
    0x26352eff, 0x53665bff, 0x62776aff,
    0x356d56ff, 0xd6e5d9ff,
    0x426b7aff, 0x14835aff, 0x356d56ff, 0xb46a10ff, 0xc9302aff, 0x5f4fe0ff,
    0x0d6b47ff, 0x8a4f00ff, 0xb3261eff, 0x355b68ff,
    0x283133ff, 0x455153ff, 0xa7b3aeff, 0xd3d7d3ff, 0x9aa6a2ff, 0x356d56ff,
    0x93aeb5ff, 0xa39fc4ff, 0xb19870ff, 0x00000047,
};

Palette g_pal;

void theme_set(int mode) {
    g_theme_mode = (mode == THEME_LIGHT) ? THEME_LIGHT : THEME_DARK;
    g_pal = (g_theme_mode == THEME_LIGHT) ? LIGHT : DARK;
}

void theme_toggle(void) {
    theme_set(g_theme_mode == THEME_LIGHT ? THEME_DARK : THEME_LIGHT);
}

int theme_is_light(void) { return g_theme_mode == THEME_LIGHT; }

/* 让静态初始化也有一份可用调色板 */
__attribute__((constructor)) static void theme_boot(void) { theme_set(THEME_DARK); }
