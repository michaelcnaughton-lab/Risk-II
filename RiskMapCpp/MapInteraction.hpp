#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mapview {
constexpr double PulseSeconds = 2.8;
constexpr double BrightnessGain = 0.30;
struct Rect { int x{}, y{}, w{}, h{}; };
inline Rect fit(int width, int height, int mapWidth, int mapHeight) {
    if (width <= 0 || height <= 0 || mapWidth <= 0 || mapHeight <= 0) return {};
    const double scale = std::min(double(width) / mapWidth, double(height) / mapHeight);
    const int w = std::max(1, int(mapWidth * scale));
    const int h = std::max(1, int(mapHeight * scale));
    return {(width-w)/2, (height-h)/2, w, h};
}
inline int hit(double x, double y, Rect view, int width, int height,
               const std::vector<std::uint8_t>& ids) {
    if (view.w <= 0 || view.h <= 0 || x < view.x || y < view.y ||
        x >= view.x+view.w || y >= view.y+view.h) return 0;
    const int mx = int((x-view.x) * width / view.w);
    const int my = int((y-view.y) * height / view.h);
    return ids.at(std::size_t(my)*width+mx);
}
struct Interaction {
    int hovered = 0, selected = 0;
    double hoverStarted = 0;
    void hover(int id, double now) {
        if (hovered != id) { hovered = id; hoverStarted = now; }
    }
    void select(int id) { selected = id; }
    void clear() { selected = 0; }
    double amount(int id, double now) const {
        if (id == 0) return 0;
        if (id == selected) return 1;  // Selection never pulses.
        if (id != hovered) return 0;
        constexpr double tau = 6.283185307179586;
        return 0.5 - 0.5 * std::cos(tau * std::max(0.0, now-hoverStarted) / PulseSeconds);
    }
};
}
