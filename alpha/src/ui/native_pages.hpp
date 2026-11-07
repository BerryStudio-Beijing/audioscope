/* AudioScope by BerryStudio 0.9 — native macOS pages (CoreGraphics/CoreText). */
#ifndef AS_UI_NATIVE_PAGES_HPP
#define AS_UI_NATIVE_PAGES_HPP

#include "app/native_model.hpp"
#include "ui/native_ui.h"

#include <CoreGraphics/CoreGraphics.h>

#include <string>


namespace audioscope {

struct NativeActions {
    bool toggle_capture = false;
    bool export_report = false;
    bool run_repair = false;
    bool run_loopback = false;
    bool toggle_theme = false;
};

struct NativeUi {
    UI ui;
    NativeModel *model = nullptr;
    NativeActions actions;
    int pending_page = -1;
};

void native_ui_init(NativeUi *ui);
void native_ui_frame(NativeUi *ui, CGContextRef ctx, double w, double h, double t);
void native_ui_mouse_move(NativeUi *ui, double x, double y);
void native_ui_mouse_down(NativeUi *ui, double x, double y);
void native_ui_mouse_up(NativeUi *ui, double x, double y);
void native_ui_key(NativeUi *ui, int key);

/* C++ convenience overloads for std::string call sites. */
inline void ui_text(UI *u, double x, double y, int font, uint32_t col,
                    const std::string &s) {
    ui_text(u, x, y, font, col, s.c_str());
}
inline void ui_text_right(UI *u, ASRect r, int font, uint32_t col,
                          const std::string &s) {
    ui_text_right(u, r, font, col, s.c_str());
}
inline void ui_text_center(UI *u, ASRect r, int font, uint32_t col,
                           const std::string &s) {
    ui_text_center(u, r, font, col, s.c_str());
}
inline void ui_text_ellipsis(UI *u, ASRect r, int font, uint32_t col,
                             const std::string &s) {
    ui_text_ellipsis(u, r, font, col, s.c_str());
}

} // namespace audioscope

#endif
