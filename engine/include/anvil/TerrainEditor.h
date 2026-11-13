#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace anvil {
class TerrainEditor {
public:
    TerrainEditor(std::size_t w, std::size_t h, float initial = 0.0f)
        : w_(w), h_(h) {
        if (h_ != 0 && w_ > std::numeric_limits<std::size_t>::max() / h_)
            throw std::length_error("Terrain dimensions overflow");
        const auto count = w_ * h_;
        if (count > v_.max_size()) throw std::length_error("Terrain is too large");
        v_.assign(count, std::isfinite(initial) ? initial : 0.0f);
    }
    bool valid() const { return w_ > 1 && h_ > 1; }
    float get(std::size_t x, std::size_t y) const {
        return x < w_ && y < h_ ? v_[y * w_ + x] : 0.0f;
    }
    bool set(std::size_t x, std::size_t y, float z) {
        if (x >= w_ || y >= h_ || !std::isfinite(z)) return false;
        v_[y * w_ + x] = z;
        return true;
    }
    void sculpt(float cx, float cy, float radius, float strength) {
        if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(radius) ||
            !std::isfinite(strength) || radius <= 0.0f) return;
        for (std::size_t y = 0; y < h_; ++y) for (std::size_t x = 0; x < w_; ++x) {
            const float dx = static_cast<float>(x) - cx, dy = static_cast<float>(y) - cy;
            const float d = std::hypot(dx, dy);
            if (d < radius) { const float t = 1.0f - d / radius; v_[y * w_ + x] += strength * t * t; }
        }
    }
    void flatten(float cx, float cy, float radius, float target, float amount = 1.0f) {
        if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(radius) ||
            !std::isfinite(target) || !std::isfinite(amount) || radius <= 0.0f) return;
        amount = std::clamp(amount, 0.0f, 1.0f);
        for (std::size_t y = 0; y < h_; ++y) for (std::size_t x = 0; x < w_; ++x) {
            const float d = std::hypot(static_cast<float>(x) - cx, static_cast<float>(y) - cy);
            if (d < radius) { const float t = (1.0f - d / radius) * amount; auto& v = v_[y * w_ + x]; v += (target - v) * t; }
        }
    }
    float sample(float x, float y) const {
        if (!valid() || !std::isfinite(x) || !std::isfinite(y)) return 0.0f;
        x = std::clamp(x, 0.0f, static_cast<float>(w_ - 1));
        y = std::clamp(y, 0.0f, static_cast<float>(h_ - 1));
        const auto x0 = static_cast<std::size_t>(x), y0 = static_cast<std::size_t>(y);
        const auto x1 = std::min(x0 + 1, w_ - 1), y1 = std::min(y0 + 1, h_ - 1);
        const float tx = x - static_cast<float>(x0), ty = y - static_cast<float>(y0);
        return (get(x0,y0)*(1-tx)+get(x1,y0)*tx)*(1-ty) + (get(x0,y1)*(1-tx)+get(x1,y1)*tx)*ty;
    }
    std::size_t width() const { return w_; }
    std::size_t height() const { return h_; }
private:
    std::size_t w_ = 0, h_ = 0;
    std::vector<float> v_;
};
} // namespace anvil
