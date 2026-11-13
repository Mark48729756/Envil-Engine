#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace anvil {

struct VegetationInstance { float x=0, y=0, z=0, scale=1.0f; std::uint32_t type=0; };
struct VegetationView { float x=0, y=0, z=0, maxDistance=250.0f, density=1.0f; };

inline std::uint32_t vegetationHash(std::uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x;
}
inline float vegetationRandom(std::uint32_t seed) { return static_cast<float>(vegetationHash(seed) & 0x00ffffffu) / 16777215.0f; }

// Deterministic CPU-side vegetation placement and distance filtering. Renderer instancing,
// GPU culling and wind animation are intentionally separate integration tasks.
class VegetationSystem {
public:
    explicit VegetationSystem(std::uint32_t seed = 0xA11F1u) : seed_(seed) {}

    std::vector<VegetationInstance> generate(float originX, float originZ, std::uint32_t count,
                                              float areaSize, std::uint32_t typeCount = 3) const {
        std::vector<VegetationInstance> out;
        out.reserve(count);
        const float area = std::max(0.0f, areaSize);
        const auto types = std::max(1u, typeCount);
        for (std::uint32_t i=0; i<count; ++i) {
            const std::uint32_t base = seed_ ^ (i * 0x9e3779b9u);
            const float rx = vegetationRandom(base), rz = vegetationRandom(base ^ 0x68bc21ebu);
            const float rs = vegetationRandom(base ^ 0x02e5be93u);
            out.push_back({originX + rx*area, 0.0f, originZ + rz*area,
                           0.75f + rs*0.5f, vegetationHash(base ^ 0x967a889bu) % types});
        }
        return out;
    }

    std::vector<VegetationInstance> cull(const std::vector<VegetationInstance>& instances, const VegetationView& view) const {
        std::vector<VegetationInstance> visible;
        const float maxD = std::max(0.0f, view.maxDistance);
        const float maxD2 = maxD * maxD;
        const float density = std::clamp(view.density, 0.0f, 1.0f);
        visible.reserve(instances.size());
        for (std::size_t i=0; i<instances.size(); ++i) {
            const auto& v = instances[i];
            const float dx=v.x-view.x, dy=v.y-view.y, dz=v.z-view.z;
            if (dx*dx+dy*dy+dz*dz > maxD2) continue;
            if (vegetationRandom(seed_ ^ static_cast<std::uint32_t>(i)*0x9e3779b9u) > density) continue;
            visible.push_back(v);
        }
        return visible;
    }
private:
    std::uint32_t seed_;
};

} // namespace anvil
