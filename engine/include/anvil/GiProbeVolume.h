#pragma once
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
namespace anvil {
struct Irradiance { float r=0, g=0, b=0; };
class GiProbeVolume {
public:
    GiProbeVolume(std::size_t x, std::size_t y, std::size_t z) : x_(x), y_(y), z_(z) {
        if ((y_ && x_ > std::numeric_limits<std::size_t>::max()/y_) ||
            (z_ && x_*y_ > std::numeric_limits<std::size_t>::max()/z_))
            throw std::length_error("GI probe dimensions overflow");
        const auto n = x_*y_*z_;
        if (n > p_.max_size()) throw std::length_error("GI probe volume is too large");
        p_.resize(n);
    }
    bool set(std::size_t x,std::size_t y,std::size_t z,Irradiance v) {
        if (x>=x_||y>=y_||z>=z_||!std::isfinite(v.r)||!std::isfinite(v.g)||!std::isfinite(v.b)) return false;
        p_[(z*y_+y)*x_+x]=v; return true;
    }
    Irradiance get(std::size_t x,std::size_t y,std::size_t z) const {
        return x<x_&&y<y_&&z<z_ ? p_[(z*y_+y)*x_+x] : Irradiance{};
    }
    std::size_t count() const { return p_.size(); }
private:
    std::size_t x_,y_,z_;
    std::vector<Irradiance> p_;
};
} // namespace anvil
