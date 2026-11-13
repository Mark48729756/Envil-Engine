#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace anvil {

struct GIColor { float r=0.0f, g=0.0f, b=0.0f; };
struct GIVector { float x=0.0f, y=1.0f, z=0.0f; };
using SH9 = std::array<GIColor, 9>;

inline GIColor operator+(GIColor a, GIColor b) { return {a.r+b.r,a.g+b.g,a.b+b.b}; }
inline GIColor operator*(GIColor a, float s) { return {a.r*s,a.g*s,a.b*s}; }

// Diffuse irradiance probe represented by nine real spherical-harmonic coefficients.
inline GIColor evaluateSH9(const SH9& sh, GIVector n) {
    const float len = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    if (len > 1e-6f) { n.x/=len; n.y/=len; n.z/=len; }
    const float x=n.x,y=n.y,z=n.z;
    const float basis[9] = {0.282095f,0.488603f*y,0.488603f*z,0.488603f*x,
        1.092548f*x*y,1.092548f*y*z,0.315392f*(3.0f*z*z-1.0f),
        1.092548f*x*z,0.546274f*(x*x-y*y)};
    GIColor out{};
    for (std::size_t i=0;i<9;++i) out=out+sh[i]*basis[i];
    return {std::max(0.0f,out.r),std::max(0.0f,out.g),std::max(0.0f,out.b)};
}

struct GIProbe { GIVector position{}; SH9 irradiance{}; float validity=1.0f; };

// Compact probe collection with validity-weighted nearest-probe blending.
class GIProbeSet {
public:
    void clear() { probes_.clear(); }
    void add(GIProbe probe) { probe.validity=std::clamp(probe.validity,0.0f,1.0f); probes_.push_back(probe); }
    std::size_t size() const { return probes_.size(); }
    GIColor sample(GIVector position, GIVector normal, float maxDistance=100.0f) const {
        GIColor sum{}; float total=0.0f;
        const float maxD=std::max(0.001f,maxDistance);
        for (const auto& p:probes_) {
            const float dx=p.position.x-position.x,dy=p.position.y-position.y,dz=p.position.z-position.z;
            const float d2=dx*dx+dy*dy+dz*dz;
            if (d2>maxD*maxD || p.validity<=0.0f) continue;
            const float w=p.validity/(0.01f+d2);
            sum=sum+evaluateSH9(p.irradiance,normal)*w; total+=w;
        }
        return total>1e-8f ? sum*(1.0f/total) : GIColor{};
    }
private: std::vector<GIProbe> probes_;
};

} // namespace anvil
