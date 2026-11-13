#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace anvil {

struct CloudParameters {
    float coverage = 0.55f;
    float density = 0.75f;
    float baseHeight = 1200.0f;
    float thickness = 1800.0f;
    float windX = 12.0f;
    float windZ = 3.0f;
    float windSpeed = 1.0f;
};

struct CloudSample { float density = 0.0f; float heightFraction = 0.0f; };

// Deterministic procedural cloud-field reference. Volumetric integration is provided by the RHI shader.
class CloudSystem {
public:
    explicit CloudSystem(CloudParameters p = {}) : params_(p) { sanitize(); }
    void setParameters(CloudParameters p) { params_ = p; sanitize(); }
    const CloudParameters& parameters() const { return params_; }

    CloudSample sample(float x, float y, float z, float time) const {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(time)) return {};
        const float h = (y - params_.baseHeight) / params_.thickness;
        if (h <= 0.0f || h >= 1.0f) return {};
        const float driftX = x - params_.windX * params_.windSpeed * time;
        const float driftZ = z - params_.windZ * params_.windSpeed * time;
        const float broad = valueNoise(driftX * 0.00042f, driftZ * 0.00042f);
        const float detail = valueNoise(driftX * 0.0017f + 19.1f, driftZ * 0.0017f - 7.3f);
        const float shape = broad * 0.78f + detail * 0.22f;
        const float vertical = std::sin(3.14159265f * h);
        const float threshold = 1.0f - params_.coverage;
        const float body = std::clamp((shape - threshold) / std::max(0.02f, params_.coverage), 0.0f, 1.0f);
        return {std::clamp(body * vertical * params_.density, 0.0f, 1.0f), h};
    }

    // Beer-Lambert approximation useful for cloud-shadow receivers; full ray marching is GPU-side.
    float shadowTransmittance(float x, float z, float time, float sunElevation, int steps = 8) const {
        const int count = std::clamp(steps, 1, 32);
        if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(time) || !std::isfinite(sunElevation)) return 1.0f;
        if (sunElevation <= 0.01f) return 1.0f;
        float opticalDepth = 0.0f;
        for (int i = 0; i < count; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            const float y = params_.baseHeight + params_.thickness * (0.15f + 0.7f * t);
            opticalDepth += sample(x + t * params_.thickness / sunElevation, y,
                                   z + t * params_.thickness * 0.15f, time).density;
        }
        return std::exp(-opticalDepth * params_.density * 0.16f / static_cast<float>(count));
    }
private:
    static float hash(int x, int z) {
        std::uint32_t n = static_cast<std::uint32_t>(x) * 0x8da6b343u ^ static_cast<std::uint32_t>(z) * 0xd8163841u;
        n = (n ^ (n >> 13u)) * 0x85ebca6bu; n ^= n >> 16u;
        return static_cast<float>(n & 0x00ffffffu) / 16777215.0f;
    }
    static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
    static float valueNoise(float x, float z) {
        if (!std::isfinite(x) || !std::isfinite(z)) return 0.0f;
        constexpr float limit = static_cast<float>(std::numeric_limits<int>::max() / 2);
        x = std::clamp(x, -limit, limit); z = std::clamp(z, -limit, limit);
        const int ix = static_cast<int>(std::floor(x)), iz = static_cast<int>(std::floor(z));
        const float fx = smooth(x - static_cast<float>(ix)), fz = smooth(z - static_cast<float>(iz));
        const float a = hash(ix, iz), b = hash(ix + 1, iz), c = hash(ix, iz + 1), d = hash(ix + 1, iz + 1);
        return (a + (b-a)*fx) * (1.0f-fz) + (c + (d-c)*fx) * fz;
    }
    void sanitize() {
        if (!std::isfinite(params_.coverage)) params_.coverage = 0.55f;
        if (!std::isfinite(params_.density)) params_.density = 0.75f;
        if (!std::isfinite(params_.thickness)) params_.thickness = 1800.0f;
        if (!std::isfinite(params_.baseHeight)) params_.baseHeight = 1200.0f;
        if (!std::isfinite(params_.windX)) params_.windX = 0.0f;
        if (!std::isfinite(params_.windZ)) params_.windZ = 0.0f;
        if (!std::isfinite(params_.windSpeed)) params_.windSpeed = 0.0f;
        params_.coverage = std::clamp(params_.coverage, 0.0f, 1.0f);
        params_.density = std::clamp(params_.density, 0.0f, 4.0f);
        params_.thickness = std::max(1.0f, params_.thickness);
        params_.baseHeight = std::max(0.0f, params_.baseHeight);
    }
    CloudParameters params_;
};

} // namespace anvil
