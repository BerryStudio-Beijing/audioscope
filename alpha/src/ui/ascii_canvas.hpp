/* AudioScope 0.1-alpha — tiny platform-neutral character canvas.
 *
 * The alpha uses this to prove the shared UI layer without committing to a
 * native windowing toolkit yet. A Direct2D/CoreGraphics Canvas can implement
 * the same drawing vocabulary later.
 */
#ifndef AS_UI_ASCII_CANVAS_HPP
#define AS_UI_ASCII_CANVAS_HPP

#include <string>
#include <string_view>
#include <vector>

namespace audioscope {

class AsciiCanvas {
public:
    AsciiCanvas(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    void clear(char c = ' ');
    void put(int x, int y, char c);
    void text(int x, int y, std::string_view s);
    void hline(int x0, int x1, int y, char c = '-');
    void vline(int x, int y0, int y1, char c = '|');
    void frame(char c = '#');

    std::string to_string() const;

private:
    int width_;
    int height_;
    std::vector<char> cells_;
};

} // namespace audioscope

#endif
