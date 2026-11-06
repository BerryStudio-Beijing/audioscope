#include "ui/ascii_canvas.hpp"

#include <algorithm>

namespace audioscope {

AsciiCanvas::AsciiCanvas(int width, int height)
    : width_(std::max(1, width)),
      height_(std::max(1, height)),
      cells_((size_t)width_ * (size_t)height_, ' ') {}

void AsciiCanvas::clear(char c) {
    std::fill(cells_.begin(), cells_.end(), c);
}

void AsciiCanvas::put(int x, int y, char c) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    cells_[(size_t)y * (size_t)width_ + (size_t)x] = c;
}

void AsciiCanvas::text(int x, int y, std::string_view s) {
    for (size_t i = 0; i < s.size(); ++i) {
        put(x + (int)i, y, s[i]);
    }
}

void AsciiCanvas::hline(int x0, int x1, int y, char c) {
    if (x1 < x0) std::swap(x0, x1);
    for (int x = x0; x <= x1; ++x) put(x, y, c);
}

void AsciiCanvas::vline(int x, int y0, int y1, char c) {
    if (y1 < y0) std::swap(y0, y1);
    for (int y = y0; y <= y1; ++y) put(x, y, c);
}

void AsciiCanvas::frame(char c) {
    hline(0, width_ - 1, 0, c);
    hline(0, width_ - 1, height_ - 1, c);
    vline(0, 0, height_ - 1, c);
    vline(width_ - 1, 0, height_ - 1, c);
}

std::string AsciiCanvas::to_string() const {
    std::string out;
    out.reserve((size_t)(width_ + 1) * (size_t)height_);
    for (int y = 0; y < height_; ++y) {
        out.append(&cells_[(size_t)y * (size_t)width_], (size_t)width_);
        out.push_back('\n');
    }
    return out;
}

} // namespace audioscope
