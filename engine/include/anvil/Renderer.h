#pragma once
#include "Camera.h"
#include "Mesh.h"
#include "Shader.h"
#include "Texture2D.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace anvil {

struct Material {
    glm::vec3 albedo{0.8f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    std::string albedoTexturePath;
    std::shared_ptr<Texture2D> albedoTexture;
    std::shared_ptr<Texture2D> normalTexture;      // linear RGB tangent-space normal map
    std::shared_ptr<Texture2D> roughnessTexture;    // linear grayscale
    std::shared_ptr<Texture2D> metallicTexture;     // linear grayscale
    std::shared_ptr<Texture2D> aoTexture;           // linear grayscale
};

struct DrawItem {
    const Mesh* mesh = nullptr;
    glm::mat4 model{1.0f};
    Material material;
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 50.0f;
    float radius = 12.0f; // smooth finite range; <= 0 keeps inverse-square-only falloff
};

struct Sun {
    glm::vec3 direction{-0.4f, -1.0f, -0.3f};
    glm::vec3 color{1.0f, 0.95f, 0.85f};
    float intensity = 2.0f;
};

struct RenderStats {
    std::uint32_t submittedObjects = 0;
    std::uint32_t cameraVisibleObjects = 0;
    std::uint32_t cameraCulledObjects = 0;
    std::uint32_t shadowVisibleObjects = 0;
    std::uint64_t gpuCacheBytes = 0;
    std::uint32_t gpuMeshCacheEntries = 0;
    std::uint32_t gpuTextureCacheEntries = 0;
};

struct RenderSettings {
    float exposure = 1.0f;
    bool  shadows = true;
    float shadowRadius = 30.0f;     // legacy single-frustum radius
    float shadowDistance = 180.0f; // maximum camera distance covered by cascaded sun shadows
    float shadowSplitLambda = 0.65f; // logarithmic/uniform cascade split blend
    float shadowBias = 0.0008f;
    bool  bloom = true;
    bool  ssao = true;
    float aoRadius = 0.85f;
    float aoStrength = 1.6f;
    float bloomThreshold = 1.0f;
    float bloomStrength = 0.08f;
    bool screenSpaceReflections = true;
    float reflectionStrength = 0.55f;
    bool distanceFog = true;
    glm::vec3 fogColor{0.55f,0.65f,0.75f};
    float fogDensity = 0.006f;
    float cloudTime = 0.0f;             // world-space volumetric cloud advection time
    bool fxaa = true;
    bool motionBlur = false;          // camera-motion blur; requires temporal view-projection history
    float motionBlurStrength = 0.65f; // 0 = off, 1 = full camera velocity
    bool taa = true;                 // depth-rejected temporal accumulation (static geometry; camera reprojection)
    float taaHistoryWeight = 0.90f;  // clamped to [0, 0.98]
    std::uint64_t gpuCacheBudgetBytes = 512ull * 1024ull * 1024ull; // RHI mesh + albedo cache budget
};

// Deferred PBR renderer:
//   sun shadow map -> G-buffer -> Cook-Torrance lighting (HDR) -> bloom -> ACES tonemap -> LDR output texture
class Renderer {
public:
    static constexpr int kMaxPointLights = 32;
    static constexpr int kBloomLevels = 5;
    static constexpr int kShadowSize = 2048;
    static constexpr int kShadowCascades = 3;

    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void resize(int width, int height);
    void render(const Camera& cam, const std::vector<DrawItem>& items,
                const std::vector<PointLight>& lights, const Sun& sun);

    // Final LDR image (RGBA8). The editor shows this inside its Viewport panel.
    unsigned outputTexture() const { return ldrColor_; }
    int width() const { return w_; }
    int height() const { return h_; }

    RenderSettings settings;
    RenderStats stats;
    bool presentToScreen = true;   // blit output to the default framebuffer (game mode)

private:
    void createTargets();
    void destroyTargets();
    void shadowPass(const Camera& cam, const std::vector<DrawItem>& items, const Sun& sun);
    void bloomPass();

    int w_, h_;
    std::unique_ptr<Shader> gbufferShader_, lightingShader_, postShader_, shadowShader_, bloomDown_, bloomUp_;
    unsigned gFbo_ = 0, gAlbedo_ = 0, gNormal_ = 0, gMaterial_ = 0, gDepth_ = 0;
    unsigned hdrFbo_ = 0, hdrColor_ = 0;
    unsigned ldrFbo_ = 0, ldrColor_ = 0;
    unsigned historyColor_ = 0, historyDepth_ = 0;
    bool hasTaaHistory_ = false;
    unsigned shadowFbo_ = 0, shadowTex_ = 0, shadowRawTex_ = 0;
    unsigned bloomFbo_[kBloomLevels] = {}, bloomTex_[kBloomLevels] = {};
    int bloomW_[kBloomLevels] = {}, bloomH_[kBloomLevels] = {};
    glm::mat4 lightVP_{1.0f};
    std::array<glm::mat4, kShadowCascades> cascadeLightVP_{};
    glm::vec3 cascadeSplits_{0.0f};
    std::array<float, kShadowCascades> cascadeTexelWorld_{};
    glm::mat4 previousViewProj_{1.0f};
    bool hasPreviousViewProj_ = false;
    unsigned emptyVao_ = 0;
};

} // namespace anvil
