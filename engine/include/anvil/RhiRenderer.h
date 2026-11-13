#pragma once
#include "Renderer.h"          // shared scene types: Material, DrawItem, PointLight, Sun, RenderSettings
#include "rhi/RHI.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace anvil {

// Deferred PBR renderer written only against the RHI (Vulkan 1.4 / 1.2 today, Direct3D 12 next):
//   sun shadow map -> G-buffer -> Cook-Torrance lighting (HDR) -> bloom -> ACES tonemap -> LDR texture -> (blit to backbuffer)
// Same look as the OpenGL `Renderer`; the OpenGL path is untouched. It never talks to OpenGL, so Mesh / Texture2D work
// without a GL context. The frame loop belongs to the caller:
//     dev.beginFrame(); bb = swapchain.acquire(); cmd.begin(); renderer.render(cmd, ..., bb); cmd.end();
//     dev.submit(cmd); swapchain.present(); dev.endFrame();
class RhiRenderer {
public:
    // `presentFormat` = swapchain format if known (pre-builds the blit pipeline); Undefined = created on first use / not needed.
    RhiRenderer(rhi::Device& device, int width, int height, rhi::TextureFormat presentFormat = rhi::TextureFormat::Undefined);
    ~RhiRenderer();
    RhiRenderer(const RhiRenderer&) = delete;
    RhiRenderer& operator=(const RhiRenderer&) = delete;

    void resize(int width, int height);

    // Records every pass into `cmd` (already begun, not yet ended). If `presentToScreen` and `backbuffer` is given, the
    // final image is also copied into it. Must be called between Device::beginFrame() and Device::submit().
    void render(rhi::CommandList& cmd, const Camera& cam, const std::vector<DrawItem>& items,
                const std::vector<PointLight>& lights, const Sun& sun,
                const std::shared_ptr<rhi::Texture>& backbuffer = {});

    // Final LDR image (RGBA8, gamma-encoded). Texture origin is top-left (Vulkan/D3D): the editor must not flip V.
    const std::shared_ptr<rhi::Texture>& outputTexture() const { return ldr_; }
    int width() const { return w_; }
    int height() const { return h_; }

    RenderSettings settings;
    RenderStats stats;
    bool presentToScreen = true;

private:
    struct GpuMesh { std::shared_ptr<rhi::Buffer> vb, ib; std::uint32_t indexCount = 0; std::uint64_t lastUse = 0; std::uint64_t bytes = 0; };
    struct GpuTexture { std::shared_ptr<rhi::Texture> tex; std::uint64_t lastUse = 0; std::uint64_t bytes = 0; };
    struct TexBind { unsigned slot; std::shared_ptr<rhi::Texture> tex; std::shared_ptr<rhi::Sampler> smp; };

    void createTargets();
    void destroyTargets();
    void createBlitPipeline(rhi::TextureFormat fmt);
    std::shared_ptr<rhi::Texture> makeTarget(int w, int h, rhi::TextureFormat f, bool depth, const char* label) const;
    std::shared_ptr<GpuMesh> gpuMesh(const Mesh& m);
    std::shared_ptr<rhi::Texture> gpuTexture(const Texture2D& t);
    static void bindOnly(rhi::CommandList& cmd, std::initializer_list<TexBind> binds);
    void fullscreenPass(rhi::CommandList& cmd, const char* label, const std::shared_ptr<rhi::Texture>& target, rhi::LoadOp load,
                        const std::shared_ptr<rhi::Pipeline>& pipe, const void* pc, std::uint32_t pcBytes);
    void prune();

    rhi::Device& dev_;
    int w_, h_;
    std::uint64_t frame_ = 0;

    std::shared_ptr<rhi::Pipeline> gbufferPipe_, shadowPipe_, lightingPipe_, bloomDownPipe_, bloomUpPipe_, postPipe_, blitPipe_;
    rhi::TextureFormat blitFormat_ = rhi::TextureFormat::Undefined;
    std::shared_ptr<rhi::Shader> blitShader_;
    std::shared_ptr<rhi::Sampler> nearestClamp_, linearClamp_, shadowSampler_, albedoSampler_;
    std::shared_ptr<rhi::Buffer> frameUbo_;
    std::shared_ptr<rhi::Texture> white_, black_;

    std::shared_ptr<rhi::Texture> gAlbedo_, gNormal_, gMaterial_, gDepth_, hdr_, ldr_, shadowTex_;
    std::shared_ptr<rhi::Texture> bloomTex_[Renderer::kBloomLevels];
    int bloomW_[Renderer::kBloomLevels] = {}, bloomH_[Renderer::kBloomLevels] = {};

    std::unordered_map<std::uint64_t, std::shared_ptr<GpuMesh>> meshes_;
    std::unordered_map<std::uint64_t, GpuTexture> textures_;
};

} // namespace anvil
