#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace anvil {

struct MaterialLayer { float height=0.0f, blendSharpness=1.0f, tiling=1.0f; std::uint32_t materialId=0; };
struct MaterialBlend { std::uint32_t materialId=0; float weight=0.0f; };

// CPU reference utilities for layered terrain/material evaluation. Texture sampling stays on GPU.
inline float parallaxOcclusionOffset(float viewX, float viewY, float heightScale,
                                     const std::vector<float>& heightSamples, std::uint32_t maxSteps=32) {
    if (heightSamples.empty() || maxSteps==0 || !std::isfinite(heightScale)) return 0.0f;
    const float vz=std::max(0.05f,std::abs(viewY));
    const float raySlope=std::abs(viewX)/vz*heightScale;
    const std::uint32_t steps=std::clamp(maxSteps,1u,128u);
    for (std::uint32_t i=0;i<steps;++i) {
        const float rayDepth=static_cast<float>(i+1)/static_cast<float>(steps);
        const float u=rayDepth;
        const auto index=std::min<std::size_t>(static_cast<std::size_t>(u*heightSamples.size()),heightSamples.size()-1);
        if (rayDepth>=std::clamp(heightSamples[index],0.0f,1.0f))
            return -raySlope*rayDepth;
    }
    return -raySlope;
}

inline std::vector<MaterialBlend> blendMaterialLayers(const std::vector<MaterialLayer>& layers, float surfaceHeight) {
    std::vector<MaterialBlend> out;
    if (layers.empty()) return out;
    out.reserve(layers.size());
    float total=0.0f;
    for (const auto& layer : layers) {
        const float sharp=std::max(0.001f,layer.blendSharpness);
        const float weight=std::clamp(1.0f-std::abs(surfaceHeight-layer.height)*sharp,0.0f,1.0f);
        out.push_back({layer.materialId,weight}); total+=weight;
    }
    if (total<=1.0e-6f) { out.front().weight=1.0f; return out; }
    for (auto& item : out) item.weight/=total;
    return out;
}

inline float triplanarWeight(float normalComponent, float sharpness=4.0f) {
    return std::pow(std::abs(normalComponent),std::max(0.001f,sharpness));
}

} // namespace anvil
