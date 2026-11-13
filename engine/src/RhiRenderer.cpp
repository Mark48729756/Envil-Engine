#include "anvil/RhiRenderer.h"
#include "anvil/ShadowMath.h"
#include "anvil/Frustum.h"
#include "anvil/Core.h"
#include "anvil/rhi/ShaderBlob.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cstring>

#ifndef ANVIL_RHI_SHADER_DIR
#define ANVIL_RHI_SHADER_DIR ""
#endif

namespace anvil {
namespace {

constexpr int kLevels = Renderer::kBloomLevels;
constexpr unsigned kClearSlots = 8;   // texture slots the renderer ever uses are 0..5

// GL clip space (z -1..1) -> Vulkan/D3D clip space (z 0..1). Applied on the left of every projection matrix.
glm::mat4 clipFix() {
    glm::mat4 m(1.0f);
    m[2][2] = 0.5f;
    m[3][2] = 0.5f;
    return m;
}

// std140, mirrored by `Frame` in shaders/rhi/lighting.frag (gbuffer.vert / shadow.vert read a prefix of it).
struct FrameData {
    glm::mat4 viewProj, lightVP, invViewProj;
    glm::vec4 camPos, sunDir, sunRadiance, fogColor;
    glm::vec4 shadowParams;   // x bias, y texel, z normal offset, w enabled
    glm::vec4 aoParams;       // x radius, y strength, z ssao enabled, w ssr enabled
    glm::vec4 ssrFog;         // x ssr strength, y fog enabled, z light count
    glm::vec4 lightPos[Renderer::kMaxPointLights];   // xyz, w radius
    glm::vec4 lightRad[Renderer::kMaxPointLights];   // rgb radiance
};
static_assert(sizeof(FrameData) == 304 + 2 * 16 * Renderer::kMaxPointLights, "FrameData must match the std140 block in lighting.frag");

struct GBufferPC { glm::mat4 model; glm::vec4 albedoRough; glm::vec4 metalMap; };   // 96 bytes
struct ShadowPC { glm::mat4 model; };                                               // 64 bytes
struct BloomDownPC { float texel[2]; std::int32_t first; float threshold; };         // 16 bytes
struct BloomUpPC { float texel[2]; };                                               // 8 bytes
struct PostPC { float invRes[2]; float exposure; float bloomStrength; std::int32_t fxaa; };   // 20 bytes
struct BlitPC { std::int32_t toLinear; };                                           // 4 bytes

bool isSrgb(rhi::TextureFormat f) { return f == rhi::TextureFormat::SBGRA8 || f == rhi::TextureFormat::SRGBA8; }

} // namespace

RhiRenderer::RhiRenderer(rhi::Device& device, int w, int h, rhi::TextureFormat presentFormat) : dev_(device), w_(std::max(1, w)), h_(std::max(1, h)) {
    using namespace rhi;
    const Backend backend = dev_.capabilities().backend;
    const std::string dir = std::string(ANVIL_RHI_SHADER_DIR).empty() ? compiledShaderDir() + "/rhi" : std::string(ANVIL_RHI_SHADER_DIR);
    auto shader = [&](const char* v, const char* f) { return dev_.createShader(loadShaderDesc(backend, dir, v, f)); };

    // ---- samplers ----
    { SamplerDesc d; d.minFilter = d.magFilter = TextureFilter::Nearest; d.wrapU = d.wrapV = TextureWrap::ClampToEdge; nearestClamp_ = dev_.createSampler(d); }
    { SamplerDesc d; d.wrapU = d.wrapV = TextureWrap::ClampToEdge; linearClamp_ = dev_.createSampler(d); }
    { SamplerDesc d; d.wrapU = d.wrapV = TextureWrap::ClampToBorder; d.whiteBorder = true; d.compare = true; d.compareOp = CompareOp::LessEqual; shadowSampler_ = dev_.createSampler(d); }
    { SamplerDesc d; d.minFilter = TextureFilter::LinearMipmapLinear; d.maxAnisotropy = std::min(8.0f, std::max(1.0f, dev_.capabilities().maxAnisotropy)); albedoSampler_ = dev_.createSampler(d); }

    // ---- per-frame constants + 1x1 fallbacks ----
    { BufferDesc d; d.size = sizeof(FrameData); d.usage = BufferUsage::Uniform; d.dynamic = true; d.label = "frame constants"; frameUbo_ = dev_.createBuffer(d); }
    const unsigned char whitePx[4] = {255, 255, 255, 255}, blackPx[4] = {0, 0, 0, 255};
    { TextureDesc d; d.format = TextureFormat::RGBA8; d.label = "white 1x1"; white_ = dev_.createTexture2D(d, whitePx); }
    { TextureDesc d; d.format = TextureFormat::RGBA8; d.label = "black 1x1"; black_ = dev_.createTexture2D(d, blackPx); }

    // ---- shadow map (hardware PCF) ----
    shadowTex_ = makeTarget(Renderer::kShadowSize, Renderer::kShadowSize, TextureFormat::Depth32F, true, "sun shadow map");

    // ---- pipelines ----
    VertexLayout meshLayout;
    meshLayout.bindings = {{0, sizeof(Vertex), false}};
    meshLayout.attributes = {{0, 0, (std::uint32_t)offsetof(Vertex, pos), VertexFormat::Float3},
                             {1, 0, (std::uint32_t)offsetof(Vertex, normal), VertexFormat::Float3},
                             {2, 0, (std::uint32_t)offsetof(Vertex, uv), VertexFormat::Float2}};
    {
        PipelineDesc d;
        d.depthTest = true; d.depthWrite = true; d.cull = false;   // like the GL path: no culling in the shadow pass
        d.depthFormat = TextureFormat::Depth32F; d.vertexLayout = meshLayout;
        d.depthBiasConstant = 4.0f; d.depthBiasSlope = 2.0f;       // glPolygonOffset(2, 4)
        d.pushConstantBytes = sizeof(ShadowPC); d.label = "shadow";
        shadowPipe_ = dev_.createPipeline(shader("shadow.vert", "shadow.frag"), d);
    }
    {
        PipelineDesc d;
        d.depthTest = true; d.depthWrite = true; d.cull = true; d.cullMode = CullMode::Back; d.frontFaceCCW = true;
        d.colorFormats = {TextureFormat::RGBA8, TextureFormat::RGBA16F, TextureFormat::RGBA8};
        d.depthFormat = TextureFormat::Depth32F; d.vertexLayout = meshLayout;
        d.pushConstantBytes = sizeof(GBufferPC); d.label = "gbuffer";
        gbufferPipe_ = dev_.createPipeline(shader("gbuffer.vert", "gbuffer.frag"), d);
    }
    auto fullscreen = [&](const char* frag, TextureFormat fmt, std::uint32_t pcBytes, const char* label, bool additive) {
        PipelineDesc d;
        d.depthTest = false; d.depthWrite = false; d.cull = false;
        d.colorFormats = {fmt}; d.pushConstantBytes = pcBytes; d.label = label;
        if (additive) { d.blend = true; d.blendMode = BlendMode::Additive; }
        return dev_.createPipeline(shader("fullscreen.vert", frag), d);
    };
    lightingPipe_  = fullscreen("lighting.frag",   TextureFormat::RGBA16F, 0, "lighting", false);
    bloomDownPipe_ = fullscreen("bloom_down.frag", TextureFormat::RGBA16F, sizeof(BloomDownPC), "bloom down", false);
    bloomUpPipe_   = fullscreen("bloom_up.frag",   TextureFormat::RGBA16F, sizeof(BloomUpPC), "bloom up", true);
    postPipe_      = fullscreen("post.frag",       TextureFormat::RGBA8, sizeof(PostPC), "post", false);
    blitShader_ = shader("fullscreen.vert", "blit.frag");
    if (presentFormat != TextureFormat::Undefined) createBlitPipeline(presentFormat);

    createTargets();
    ANVIL_LOG("RhiRenderer: %dx%d on %s (%s)", w_, h_, dev_.capabilities().deviceName.c_str(), rhi::backendName(backend));
}

RhiRenderer::~RhiRenderer() {
    dev_.waitIdle();
}

void RhiRenderer::createBlitPipeline(rhi::TextureFormat fmt) {
    rhi::PipelineDesc d;
    d.depthTest = false; d.depthWrite = false; d.cull = false;
    d.colorFormats = {fmt}; d.pushConstantBytes = sizeof(BlitPC); d.label = "present blit";
    blitPipe_ = dev_.createPipeline(blitShader_, d);
    blitFormat_ = fmt;
}

std::shared_ptr<rhi::Texture> RhiRenderer::makeTarget(int w, int h, rhi::TextureFormat f, bool depth, const char* label) const {
    rhi::TextureDesc d;
    d.width = w; d.height = h; d.format = f; d.label = label;
    d.usage = rhi::TextureUsage::Sampled | (depth ? rhi::TextureUsage::DepthStencil : rhi::TextureUsage::RenderTarget);
    return dev_.createTexture2D(d);
}

void RhiRenderer::createTargets() {
    using rhi::TextureFormat;
    gAlbedo_   = makeTarget(w_, h_, TextureFormat::RGBA8,    false, "gbuffer albedo");
    gNormal_   = makeTarget(w_, h_, TextureFormat::RGBA16F,  false, "gbuffer normal");
    gMaterial_ = makeTarget(w_, h_, TextureFormat::RGBA8,    false, "gbuffer material");
    gDepth_    = makeTarget(w_, h_, TextureFormat::Depth32F, true,  "gbuffer depth");
    hdr_       = makeTarget(w_, h_, TextureFormat::RGBA16F,  false, "hdr color");
    ldr_       = makeTarget(w_, h_, TextureFormat::RGBA8,    false, "ldr color");
    int bw = w_, bh = h_;
    for (int i = 0; i < kLevels; ++i) {
        bw = std::max(1, bw / 2); bh = std::max(1, bh / 2);
        bloomW_[i] = bw; bloomH_[i] = bh;
        bloomTex_[i] = makeTarget(bw, bh, TextureFormat::RGBA16F, false, "bloom");
    }
}

void RhiRenderer::destroyTargets() {
    gAlbedo_.reset(); gNormal_.reset(); gMaterial_.reset(); gDepth_.reset(); hdr_.reset(); ldr_.reset();
    for (auto& t : bloomTex_) t.reset();
}

void RhiRenderer::resize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == w_ && h == h_)) return;
    dev_.waitIdle();   // rare; keeps the old targets alive until the GPU is done with them
    destroyTargets();
    w_ = w; h_ = h;
    createTargets();
}

std::shared_ptr<RhiRenderer::GpuMesh> RhiRenderer::gpuMesh(const Mesh& m) {
    auto it = meshes_.find(m.uid());
    if (it != meshes_.end()) { it->second->lastUse = frame_; return it->second; }
    auto g = std::make_shared<GpuMesh>();
    rhi::BufferDesc vb; vb.size = m.vertices().size() * sizeof(Vertex); vb.usage = rhi::BufferUsage::Vertex; vb.label = "mesh vertices";
    rhi::BufferDesc ib; ib.size = m.indices().size() * sizeof(std::uint32_t); ib.usage = rhi::BufferUsage::Index; ib.label = "mesh indices";
    g->vb = dev_.createBuffer(vb, m.vertices().data());
    g->ib = dev_.createBuffer(ib, m.indices().data());
    g->indexCount = (std::uint32_t)m.indices().size();
    g->lastUse = frame_;
    g->bytes = vb.size + ib.size;
    meshes_[m.uid()] = g;
    return g;
}

std::shared_ptr<rhi::Texture> RhiRenderer::gpuTexture(const Texture2D& t) {
    auto it = textures_.find(t.uid());
    if (it != textures_.end()) { it->second.lastUse = frame_; return it->second.tex; }
    const std::size_t n = (std::size_t)t.width() * (std::size_t)t.height();
    std::vector<unsigned char> rgba(n * 4, 255);
    const auto& rgb = t.rgb();
    for (std::size_t i = 0; i < n; ++i) { rgba[i * 4] = rgb[i * 3]; rgba[i * 4 + 1] = rgb[i * 3 + 1]; rgba[i * 4 + 2] = rgb[i * 3 + 2]; }
    rhi::TextureDesc d;
    d.width = t.width(); d.height = t.height();
    d.levels = 1 + (int)std::floor(std::log2((double)std::max(t.width(), t.height())));
    d.format = t.srgb() ? rhi::TextureFormat::SRGBA8 : rhi::TextureFormat::RGBA8;
    d.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::TransferDst | rhi::TextureUsage::TransferSrc;
    d.label = "albedo map";
    auto tex = dev_.createTexture2D(d, rgba.data());
    const std::uint64_t mipFactorNumerator = 4;
    const std::uint64_t mipFactorDenominator = 3;
    const std::uint64_t baseBytes = static_cast<std::uint64_t>(t.width()) * static_cast<std::uint64_t>(t.height()) * 4ull;
    textures_[t.uid()] = {tex, frame_, (baseBytes * mipFactorNumerator + mipFactorDenominator - 1) / mipFactorDenominator};
    return tex;
}

void RhiRenderer::prune() {
    // Budget-driven LRU eviction. Prepared draw records own shared_ptrs, so resources used by this frame are pinned.
    std::uint64_t total = 0;
    for (const auto& entry : meshes_) total += entry.second->bytes;
    for (const auto& entry : textures_) total += entry.second.bytes;
    const std::uint64_t budget = std::max<std::uint64_t>(settings.gpuCacheBudgetBytes, 16ull * 1024ull * 1024ull);
    const bool periodicSweep = (frame_ % 120u) == 0u;
    for (;;) {
        const bool overBudget = total > budget;
        if (!overBudget && !periodicSweep) break;
        bool meshCandidate = false;
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t candidateId = 0;
        for (const auto& entry : meshes_) {
            const bool stale = entry.second->lastUse + 300u < frame_;
            if (entry.second.use_count() == 1 && (overBudget || stale) && entry.second->lastUse < oldest) {
                oldest = entry.second->lastUse; candidateId = entry.first; meshCandidate = true;
            }
        }
        for (const auto& entry : textures_) {
            const bool stale = entry.second.lastUse + 300u < frame_;
            if (entry.second.tex.use_count() == 1 && (overBudget || stale) && entry.second.lastUse < oldest) {
                oldest = entry.second.lastUse; candidateId = entry.first; meshCandidate = false;
            }
        }
        if (oldest == std::numeric_limits<std::uint64_t>::max()) break;
        if (meshCandidate) {
            auto it = meshes_.find(candidateId);
            if (it != meshes_.end()) { total -= std::min(total, it->second->bytes); meshes_.erase(it); }
        } else {
            auto it = textures_.find(candidateId);
            if (it != textures_.end()) { total -= std::min(total, it->second.bytes); textures_.erase(it); }
        }
    }
    stats.gpuCacheBytes = total;
    stats.gpuMeshCacheEntries = static_cast<std::uint32_t>(meshes_.size());
    stats.gpuTextureCacheEntries = static_cast<std::uint32_t>(textures_.size());
}

// Binds exactly the listed textures and clears every other slot. A stale binding of a texture that is a render target
// in the next pass would otherwise force a layout change in the middle of it.
void RhiRenderer::bindOnly(rhi::CommandList& cmd, std::initializer_list<TexBind> binds) {
    for (unsigned s = 0; s < kClearSlots; ++s) {
        const TexBind* hit = nullptr;
        for (const TexBind& b : binds) if (b.slot == s) hit = &b;
        if (hit) cmd.bindTexture(s, hit->tex, hit->smp);
        else cmd.bindTexture(s, std::shared_ptr<rhi::Texture>{}, std::shared_ptr<rhi::Sampler>{});
    }
}

void RhiRenderer::fullscreenPass(rhi::CommandList& cmd, const char* label, const std::shared_ptr<rhi::Texture>& target, rhi::LoadOp load,
                                 const std::shared_ptr<rhi::Pipeline>& pipe, const void* pc, std::uint32_t pcBytes) {
    rhi::RenderPassDesc rp; rp.label = label;
    rhi::ColorAttachment ca; ca.texture = target; ca.load = load;
    rp.colors.push_back(ca);
    cmd.beginRenderPass(rp);
    cmd.setPipeline(pipe);
    cmd.bindUniformBuffer(0, frameUbo_);
    if (pc && pcBytes) cmd.pushConstants(pc, pcBytes, 0);
    cmd.draw(3);
    cmd.endRenderPass();
}

void RhiRenderer::render(rhi::CommandList& cmd, const Camera& cam, const std::vector<DrawItem>& items,
                         const std::vector<PointLight>& lights, const Sun& sun, const std::shared_ptr<rhi::Texture>& backbuffer) {
    using namespace rhi;
    ++frame_;
    const glm::mat4 fix = clipFix();
    const glm::mat4 vp = fix * cam.proj((float)w_ / (float)h_) * cam.view();
    const Frustum cameraFrustum = Frustum::fromViewProjection(vp, true);
    stats = {};
    stats.submittedObjects = static_cast<std::uint32_t>(items.size());

    // Shared stabilized directional-light shadow camera; only clip-space depth differs by API.
    const ShadowFrustum shadow = buildShadowFrustum(cam.position, cam.forward(), sun.direction,
                                                     settings.shadowRadius, Renderer::kShadowSize, true);
    const glm::vec3 dir = shadow.direction;
    const float shadowTexelWorld = shadow.texelWorld;
    const glm::mat4 lightVP = shadow.lightVP;
    const Frustum lightFrustum = Frustum::fromViewProjection(lightVP, true);

    // ---- one constant block for the whole frame (dynamic buffers hold one value per frame) ----
    FrameData fd{};
    fd.viewProj = vp; fd.lightVP = lightVP; fd.invViewProj = glm::inverse(vp);
    fd.camPos = glm::vec4(cam.position, 1.0f);
    fd.sunDir = glm::vec4(dir, 0.0f);
    fd.sunRadiance = glm::vec4(sun.color * sun.intensity, 0.0f);
    fd.fogColor = glm::vec4(settings.fogColor, settings.fogDensity);
    fd.shadowParams = glm::vec4(settings.shadowBias, 1.0f / (float)Renderer::kShadowSize, shadowTexelWorld * 1.5f, settings.shadows ? 1.0f : 0.0f);
    fd.aoParams = glm::vec4(settings.aoRadius, settings.aoStrength, settings.ssao ? 1.0f : 0.0f, settings.screenSpaceReflections ? 1.0f : 0.0f);
    const int n = (int)std::min<std::size_t>(lights.size(), Renderer::kMaxPointLights);
    fd.ssrFog = glm::vec4(settings.reflectionStrength, settings.distanceFog ? 1.0f : 0.0f, (float)n, 0.0f);
    for (int i = 0; i < n; ++i) {
        fd.lightPos[i] = glm::vec4(lights[i].position, lights[i].radius);
        fd.lightRad[i] = glm::vec4(lights[i].color * lights[i].intensity, 0.0f);
    }
    frameUbo_->update(0, &fd, sizeof fd);

    // ---- GPU copies of meshes / textures (uploaded on first use) ----
    struct Prepared { std::shared_ptr<GpuMesh> mesh; std::shared_ptr<Texture> albedo; const DrawItem* item; bool cameraVisible; bool shadowVisible; };
    std::vector<Prepared> draws;
    draws.reserve(items.size());
    for (const DrawItem& it : items) {
        if (!it.mesh) continue;
        glm::vec3 center; float radius = 0.0f;
        transformSphere(it.model, it.mesh->boundsCenter(), it.mesh->boundsRadius(), center, radius);
        const bool cameraVisible = cameraFrustum.intersectsSphere(center, radius);
        const bool shadowVisible = settings.shadows && lightFrustum.intersectsSphere(center, radius);
        if (cameraVisible) ++stats.cameraVisibleObjects; else ++stats.cameraCulledObjects;
        if (shadowVisible) ++stats.shadowVisibleObjects;
        if (!cameraVisible && !shadowVisible) continue;
        Prepared p{gpuMesh(*it.mesh), cameraVisible && it.material.albedoTexture ? gpuTexture(*it.material.albedoTexture) : nullptr, &it, cameraVisible, shadowVisible};
        draws.push_back(std::move(p));
    }
    prune();

    auto drawMesh = [&](const GpuMesh& g) {
        cmd.bindVertexBuffer(g.vb, 0);
        cmd.bindIndexBuffer(g.ib);
        cmd.drawIndexed(g.indexCount, 0);
    };

    // 1) sun shadow map
    if (settings.shadows) {
        bindOnly(cmd, {});
        RenderPassDesc rp; rp.label = "Shadow";
        rp.depth.texture = shadowTex_; rp.depth.load = LoadOp::Clear; rp.depth.clearDepth = 1.0f;
        cmd.beginRenderPass(rp);
        cmd.setPipeline(shadowPipe_);
        cmd.bindUniformBuffer(0, frameUbo_);
        for (const Prepared& p : draws) {
            if (!p.shadowVisible) continue;
            ShadowPC pc{p.item->model};
            cmd.pushConstants(&pc, sizeof pc, 0);
            drawMesh(*p.mesh);
        }
        cmd.endRenderPass();
    }

    // 2) G-buffer
    {
        bindOnly(cmd, {{5, white_, albedoSampler_}});
        RenderPassDesc rp; rp.label = "G-buffer";
        for (const auto& t : {gAlbedo_, gNormal_, gMaterial_}) { ColorAttachment ca; ca.texture = t; ca.load = LoadOp::Clear; rp.colors.push_back(ca); }
        rp.depth.texture = gDepth_; rp.depth.load = LoadOp::Clear; rp.depth.clearDepth = 1.0f;
        cmd.beginRenderPass(rp);
        cmd.setPipeline(gbufferPipe_);
        cmd.bindUniformBuffer(0, frameUbo_);
        const Texture* bound = white_.get();
        for (const Prepared& p : draws) {
            if (!p.cameraVisible) continue;
            const Material& m = p.item->material;
            GBufferPC pc{p.item->model, glm::vec4(m.albedo, m.roughness), glm::vec4(m.metallic, p.albedo ? 1.0f : 0.0f, 0.0f, 0.0f)};
            const std::shared_ptr<Texture>& want = p.albedo ? p.albedo : white_;
            if (want.get() != bound) { cmd.bindTexture(5, want, albedoSampler_); bound = want.get(); }
            cmd.pushConstants(&pc, sizeof pc, 0);
            drawMesh(*p.mesh);
        }
        cmd.endRenderPass();
    }

    // 3) lighting -> HDR
    bindOnly(cmd, {{0, gAlbedo_, nearestClamp_}, {1, gNormal_, nearestClamp_}, {2, gMaterial_, nearestClamp_},
                   {3, gDepth_, nearestClamp_}, {4, shadowTex_, shadowSampler_}});
    fullscreenPass(cmd, "Lighting", hdr_, LoadOp::Clear, lightingPipe_, nullptr, 0);

    // 4) bloom: downsample chain (first level thresholds), then additive upsample chain
    if (settings.bloom) {
        for (int i = 0; i < kLevels; ++i) {
            const auto& src = (i == 0) ? hdr_ : bloomTex_[i - 1];
            const int sw = (i == 0) ? w_ : bloomW_[i - 1], sh = (i == 0) ? h_ : bloomH_[i - 1];
            BloomDownPC pc{{1.0f / (float)sw, 1.0f / (float)sh}, i == 0 ? 1 : 0, settings.bloomThreshold};
            bindOnly(cmd, {{0, src, linearClamp_}});
            fullscreenPass(cmd, "Bloom down", bloomTex_[i], LoadOp::Clear, bloomDownPipe_, &pc, sizeof pc);
        }
        for (int i = kLevels - 2; i >= 0; --i) {
            BloomUpPC pc{{1.0f / (float)bloomW_[i + 1], 1.0f / (float)bloomH_[i + 1]}};
            bindOnly(cmd, {{0, bloomTex_[i + 1], linearClamp_}});
            fullscreenPass(cmd, "Bloom up", bloomTex_[i], LoadOp::Load, bloomUpPipe_, &pc, sizeof pc);
        }
    }

    // 5) tonemap (+FXAA) -> LDR
    {
        PostPC pc{{1.0f / (float)w_, 1.0f / (float)h_}, settings.exposure, settings.bloom ? settings.bloomStrength : 0.0f, settings.fxaa ? 1 : 0};
        bindOnly(cmd, {{0, hdr_, linearClamp_}, {1, settings.bloom ? bloomTex_[0] : black_, linearClamp_}});
        fullscreenPass(cmd, "Tonemap", ldr_, LoadOp::Clear, postPipe_, &pc, sizeof pc);
    }

    // 6) copy to the swapchain image
    if (presentToScreen && backbuffer) {
        if (!blitPipe_ || backbuffer->format() != blitFormat_) createBlitPipeline(backbuffer->format());
        BlitPC pc{isSrgb(backbuffer->format()) ? 1 : 0};
        bindOnly(cmd, {{0, ldr_, linearClamp_}});
        fullscreenPass(cmd, "Present", backbuffer, LoadOp::Clear, blitPipe_, &pc, sizeof pc);
    }
    bindOnly(cmd, {});
}

} // namespace anvil
