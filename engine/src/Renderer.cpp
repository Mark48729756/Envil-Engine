#include "anvil/Renderer.h"
#include "anvil/ShadowMath.h"
#include "anvil/Frustum.h"
#include "anvil/Core.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace anvil {

// Portable shader lookup: $ANVIL_SHADER_DIR, <exe dir>/shaders, ./shaders, then the build-time path.
static std::string resolveShaderDir() {
    namespace fs = std::filesystem;
    auto ok = [](const fs::path& p) { std::error_code ec; return fs::exists(p / "gbuffer.vert", ec); };
    if (const char* e = std::getenv("ANVIL_SHADER_DIR")) if (ok(e)) return std::string(e) + "/";
    fs::path exeDir;
#if defined(_WIN32)
    char buf[MAX_PATH]; DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) exeDir = fs::path(buf).parent_path();
#else
    std::error_code ec; fs::path exe = fs::read_symlink("/proc/self/exe", ec); if (!ec) exeDir = exe.parent_path();
#endif
    if (!exeDir.empty()) {
        for (fs::path d = exeDir, i = d; ; ) { if (ok(d / "shaders")) return (d / "shaders").string() + "/"; if (!d.has_parent_path() || d.parent_path() == d) break; d = d.parent_path(); (void)i; }
    }
    if (ok("shaders")) return "shaders/";
    return std::string(ANVIL_SHADER_DIR) + "/";
}
static const std::string kDir = resolveShaderDir();

Renderer::Renderer(int w, int h) : w_(w), h_(h) {
    gbufferShader_  = std::make_unique<Shader>(kDir + "gbuffer.vert", kDir + "gbuffer.frag");
    lightingShader_ = std::make_unique<Shader>(kDir + "fullscreen.vert", kDir + "lighting.frag");
    postShader_     = std::make_unique<Shader>(kDir + "fullscreen.vert", kDir + "post.frag");
    shadowShader_   = std::make_unique<Shader>(kDir + "shadow.vert", kDir + "shadow.frag");
    bloomDown_      = std::make_unique<Shader>(kDir + "fullscreen.vert", kDir + "bloom_down.frag");
    bloomUp_        = std::make_unique<Shader>(kDir + "fullscreen.vert", kDir + "bloom_up.frag");
    glCreateVertexArrays(1, &emptyVao_);

    // sun shadow map (hardware PCF comparison sampler)
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadowTex_);
    glTextureStorage3D(shadowTex_, 1, GL_DEPTH_COMPONENT32F, kShadowSize, kShadowSize, kShadowCascades);
    glTextureParameteri(shadowTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadowTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(shadowTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1, 1, 1, 1};
    glTextureParameterfv(shadowTex_, GL_TEXTURE_BORDER_COLOR, border);
    glTextureParameteri(shadowTex_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(shadowTex_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    // Same immutable depth storage exposed through a non-comparison view for PCSS blocker search.
    glGenTextures(1, &shadowRawTex_);
    glTextureView(shadowRawTex_, GL_TEXTURE_2D_ARRAY, shadowTex_, GL_DEPTH_COMPONENT32F, 0, 1, 0, kShadowCascades);
    glTextureParameteri(shadowRawTex_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(shadowRawTex_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(shadowRawTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowRawTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowRawTex_, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTextureParameterfv(shadowRawTex_, GL_TEXTURE_BORDER_COLOR, border);
    glCreateFramebuffers(1, &shadowFbo_);
    glNamedFramebufferTextureLayer(shadowFbo_, GL_DEPTH_ATTACHMENT, shadowTex_, 0, 0);
    glNamedFramebufferDrawBuffer(shadowFbo_, GL_NONE);
    glNamedFramebufferReadBuffer(shadowFbo_, GL_NONE);
    ANVIL_ASSERT(glCheckNamedFramebufferStatus(shadowFbo_, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Shadow map incomplete");

    createTargets();
}

Renderer::~Renderer() {
    destroyTargets();
    glDeleteTextures(1, &shadowTex_);
    glDeleteTextures(1, &shadowRawTex_);
    glDeleteFramebuffers(1, &shadowFbo_);
    glDeleteVertexArrays(1, &emptyVao_);
}

void Renderer::createTargets() {
    auto tex = [&](GLuint& t, GLenum fmt, int w, int h, GLenum filter) {
        glCreateTextures(GL_TEXTURE_2D, 1, &t);
        glTextureStorage2D(t, 1, fmt, w, h);
        glTextureParameteri(t, GL_TEXTURE_MIN_FILTER, filter);
        glTextureParameteri(t, GL_TEXTURE_MAG_FILTER, filter);
        glTextureParameteri(t, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(t, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    tex(gAlbedo_, GL_RGBA8, w_, h_, GL_NEAREST);
    tex(gNormal_, GL_RGBA16F, w_, h_, GL_NEAREST);
    tex(gMaterial_, GL_RGBA8, w_, h_, GL_NEAREST);
    tex(gDepth_, GL_DEPTH_COMPONENT32F, w_, h_, GL_NEAREST);
    tex(hdrColor_, GL_RGBA16F, w_, h_, GL_LINEAR);
    tex(ldrColor_, GL_RGBA8, w_, h_, GL_LINEAR);
    tex(historyColor_, GL_RGBA8, w_, h_, GL_LINEAR);
    tex(historyDepth_, GL_DEPTH_COMPONENT32F, w_, h_, GL_NEAREST);

    glCreateFramebuffers(1, &gFbo_);
    glNamedFramebufferTexture(gFbo_, GL_COLOR_ATTACHMENT0, gAlbedo_, 0);
    glNamedFramebufferTexture(gFbo_, GL_COLOR_ATTACHMENT1, gNormal_, 0);
    glNamedFramebufferTexture(gFbo_, GL_COLOR_ATTACHMENT2, gMaterial_, 0);
    glNamedFramebufferTexture(gFbo_, GL_DEPTH_ATTACHMENT, gDepth_, 0);
    const GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glNamedFramebufferDrawBuffers(gFbo_, 3, bufs);
    ANVIL_ASSERT(glCheckNamedFramebufferStatus(gFbo_, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "G-buffer incomplete");

    glCreateFramebuffers(1, &hdrFbo_);
    glNamedFramebufferTexture(hdrFbo_, GL_COLOR_ATTACHMENT0, hdrColor_, 0);
    ANVIL_ASSERT(glCheckNamedFramebufferStatus(hdrFbo_, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "HDR target incomplete");

    glCreateFramebuffers(1, &ldrFbo_);
    glNamedFramebufferTexture(ldrFbo_, GL_COLOR_ATTACHMENT0, ldrColor_, 0);
    ANVIL_ASSERT(glCheckNamedFramebufferStatus(ldrFbo_, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "LDR target incomplete");

    int bw = w_, bh = h_;
    for (int i = 0; i < kBloomLevels; ++i) {
        bw = std::max(1, bw / 2); bh = std::max(1, bh / 2);
        bloomW_[i] = bw; bloomH_[i] = bh;
        tex(bloomTex_[i], GL_RGBA16F, bw, bh, GL_LINEAR);
        glCreateFramebuffers(1, &bloomFbo_[i]);
        glNamedFramebufferTexture(bloomFbo_[i], GL_COLOR_ATTACHMENT0, bloomTex_[i], 0);
    }
}

void Renderer::destroyTargets() {
    GLuint texs[] = {gAlbedo_, gNormal_, gMaterial_, gDepth_, hdrColor_, ldrColor_, historyColor_, historyDepth_};
    glDeleteTextures(8, texs);
    glDeleteFramebuffers(1, &gFbo_);
    glDeleteFramebuffers(1, &hdrFbo_);
    glDeleteFramebuffers(1, &ldrFbo_);
    glDeleteTextures(kBloomLevels, bloomTex_);
    glDeleteFramebuffers(kBloomLevels, bloomFbo_);
}

void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == w_ && h == h_)) return;
    destroyTargets();
    w_ = w; h_ = h;
    hasPreviousViewProj_ = false;
    hasTaaHistory_ = false;
    createTargets();
}

void Renderer::shadowPass(const Camera& cam, const std::vector<DrawItem>& items, const Sun& sun) {
    const ShadowCascadeSet cascades = buildShadowCascades(cam, static_cast<float>(w_) / static_cast<float>(h_),
        sun.direction, settings.shadowDistance, kShadowSize, settings.shadowSplitLambda);
    cascadeLightVP_ = cascades.lightVP;
    cascadeSplits_ = cascades.splitDepths;
    cascadeTexelWorld_ = cascades.texelWorld;
    lightVP_ = cascadeLightVP_[0];

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, kShadowSize, kShadowSize);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    shadowShader_->bind();
    for (int cascade = 0; cascade < kShadowCascades; ++cascade) {
        glNamedFramebufferTextureLayer(shadowFbo_, GL_DEPTH_ATTACHMENT, shadowTex_, 0, cascade);
        const float one = 1.0f;
        glClearNamedFramebufferfv(shadowFbo_, GL_DEPTH, 0, &one);
        shadowShader_->set("uLightVP", cascadeLightVP_[cascade]);
        const Frustum lightFrustum = Frustum::fromViewProjection(cascadeLightVP_[cascade]);
        for (const DrawItem& it : items) {
            if (!it.mesh) continue;
            glm::vec3 center; float radius = 0.0f;
            transformSphere(it.model, it.mesh->boundsCenter(), it.mesh->boundsRadius(), center, radius);
            if (!lightFrustum.intersectsSphere(center, radius)) continue;
            ++stats.shadowVisibleObjects;
            shadowShader_->set("uModel", it.model);
            it.mesh->draw();
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_DEPTH_TEST);
}

void Renderer::bloomPass() {
    glBindVertexArray(emptyVao_);
    glDisable(GL_BLEND);
    // downsample chain (first level applies the brightness threshold)
    bloomDown_->bind();
    bloomDown_->set("uThreshold", settings.bloomThreshold);
    for (int i = 0; i < kBloomLevels; ++i) {
        const unsigned src = (i == 0) ? hdrColor_ : bloomTex_[i - 1];
        const int sw = (i == 0) ? w_ : bloomW_[i - 1];
        const int sh = (i == 0) ? h_ : bloomH_[i - 1];
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glViewport(0, 0, bloomW_[i], bloomH_[i]);
        glBindTextureUnit(0, src);
        bloomDown_->set("uTexel", glm::vec2(1.0f / sw, 1.0f / sh));
        bloomDown_->set("uFirst", i == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    // upsample chain, additive
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    bloomUp_->bind();
    for (int i = kBloomLevels - 2; i >= 0; --i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glViewport(0, 0, bloomW_[i], bloomH_[i]);
        glBindTextureUnit(0, bloomTex_[i + 1]);
        bloomUp_->set("uTexel", glm::vec2(1.0f / bloomW_[i + 1], 1.0f / bloomH_[i + 1]));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);
}

void Renderer::render(const Camera& cam, const std::vector<DrawItem>& items,
                      const std::vector<PointLight>& lights, const Sun& sun) {
    const glm::mat4 vp = cam.proj((float)w_ / (float)h_) * cam.view();
    if (hasPreviousViewProj_) {
        float matrixDelta = 0.0f;
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
            matrixDelta = std::max(matrixDelta, std::abs(vp[c][r] - previousViewProj_[c][r]));
        if (matrixDelta > 2.0f) hasTaaHistory_ = false; // camera cut/teleport: never blend unrelated frames
    }
    const Frustum cameraFrustum = Frustum::fromViewProjection(vp);
    stats = {};
    stats.submittedObjects = static_cast<std::uint32_t>(items.size());
    for (const DrawItem& it : items) {
        if (!it.mesh) continue;
        glm::vec3 center; float radius = 0.0f;
        transformSphere(it.model, it.mesh->boundsCenter(), it.mesh->boundsRadius(), center, radius);
        if (cameraFrustum.intersectsSphere(center, radius)) ++stats.cameraVisibleObjects;
        else ++stats.cameraCulledObjects;
    }

    // 0) sun shadow map
    if (settings.shadows) shadowPass(cam, items, sun);

    // 1) geometry pass
    glViewport(0, 0, w_, h_);
    glBindFramebuffer(GL_FRAMEBUFFER, gFbo_);
    const float zero[4] = {0, 0, 0, 0};
    glClearNamedFramebufferfv(gFbo_, GL_COLOR, 0, zero);
    glClearNamedFramebufferfv(gFbo_, GL_COLOR, 1, zero);
    glClearNamedFramebufferfv(gFbo_, GL_COLOR, 2, zero);
    const float one = 1.0f;
    glClearNamedFramebufferfv(gFbo_, GL_DEPTH, 0, &one);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    gbufferShader_->bind();
    gbufferShader_->set("uViewProj", vp);
    for (const DrawItem& it : items) {
        if (!it.mesh) continue;
        glm::vec3 center; float radius = 0.0f;
        transformSphere(it.model, it.mesh->boundsCenter(), it.mesh->boundsRadius(), center, radius);
        if (!cameraFrustum.intersectsSphere(center, radius)) continue;
        gbufferShader_->set("uModel", it.model);
        gbufferShader_->set("uAlbedo", it.material.albedo);
        gbufferShader_->set("uRoughness", it.material.roughness);
        gbufferShader_->set("uMetallic", it.material.metallic);
        gbufferShader_->set("uHasAlbedoMap", it.material.albedoTexture ? 1 : 0);
        gbufferShader_->set("uHasNormalMap", it.material.normalTexture ? 1 : 0);
        gbufferShader_->set("uHasRoughnessMap", it.material.roughnessTexture ? 1 : 0);
        gbufferShader_->set("uHasMetallicMap", it.material.metallicTexture ? 1 : 0);
        gbufferShader_->set("uHasAOMap", it.material.aoTexture ? 1 : 0);
        if (it.material.albedoTexture) glBindTextureUnit(5, it.material.albedoTexture->id());
        if (it.material.normalTexture) glBindTextureUnit(6, it.material.normalTexture->id());
        if (it.material.roughnessTexture) glBindTextureUnit(7, it.material.roughnessTexture->id());
        if (it.material.metallicTexture) glBindTextureUnit(8, it.material.metallicTexture->id());
        if (it.material.aoTexture) glBindTextureUnit(9, it.material.aoTexture->id());
        it.mesh->draw();
    }
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    // 2) lighting pass -> HDR
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_);
    glBindTextureUnit(0, gAlbedo_);
    glBindTextureUnit(1, gNormal_);
    glBindTextureUnit(2, gMaterial_);
    glBindTextureUnit(3, gDepth_);
    glBindTextureUnit(4, shadowTex_);
    glBindTextureUnit(13, shadowRawTex_);
    lightingShader_->bind();
    lightingShader_->set("uInvViewProj", glm::inverse(vp));
    lightingShader_->set("uViewProj", vp);
    lightingShader_->set("uSSAOEnabled", settings.ssao ? 1 : 0);
    lightingShader_->set("uAORadius", settings.aoRadius);
    lightingShader_->set("uAOStrength", settings.aoStrength);
    lightingShader_->set("uCamPos", cam.position);
    lightingShader_->set("uSunDir", glm::normalize(sun.direction));
    lightingShader_->set("uSunRadiance", sun.color * sun.intensity);
    lightingShader_->set("uCloudTime", settings.cloudTime);
    lightingShader_->set("uShadowsOn", settings.shadows ? 1 : 0);
    lightingShader_->setArray("uLightVP[0]", cascadeLightVP_.data(), kShadowCascades);
    lightingShader_->set("uCascadeSplits", cascadeSplits_);
    lightingShader_->set("uCameraView", cam.view());
    lightingShader_->set("uShadowBias", settings.shadowBias);
    lightingShader_->set("uShadowTexel", 1.0f / (float)kShadowSize);
    lightingShader_->set("uShadowLightSize", 0.035f);
    lightingShader_->setArray("uShadowTexelWorld[0]", cascadeTexelWorld_.data(), kShadowCascades);
    lightingShader_->set("uSSREnabled", settings.screenSpaceReflections ? 1 : 0);
    lightingShader_->set("uSSRStrength", settings.reflectionStrength);
    lightingShader_->set("uFogEnabled", settings.distanceFog ? 1 : 0);
    lightingShader_->set("uFogColor", settings.fogColor);
    lightingShader_->set("uFogDensity", settings.fogDensity);
    int n = (int)std::min<size_t>(lights.size(), kMaxPointLights);
    glm::vec3 pos[kMaxPointLights], rad[kMaxPointLights];
    float radius[kMaxPointLights];
    for (int i = 0; i < n; ++i) { pos[i] = lights[i].position; rad[i] = lights[i].color * lights[i].intensity; radius[i] = lights[i].radius; }
    lightingShader_->set("uLightCount", n);
    if (n > 0) {
        lightingShader_->setArray("uLightPos", pos, n);
        lightingShader_->setArray("uLightRadiance", rad, n);
        lightingShader_->setArray("uLightRadius", radius, n);
    }
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 3) bloom
    if (settings.bloom) bloomPass();

    // 4) tonemap -> LDR output texture
    glBindFramebuffer(GL_FRAMEBUFFER, ldrFbo_);
    glViewport(0, 0, w_, h_);
    glBindTextureUnit(0, hdrColor_);
    glBindTextureUnit(1, bloomTex_[0]);
    glBindTextureUnit(2, gDepth_);
    glBindTextureUnit(3, historyColor_);
    glBindTextureUnit(4, historyDepth_);
    postShader_->bind();
    postShader_->set("uExposure", settings.exposure);
    postShader_->set("uBloomStrength", settings.bloom ? settings.bloomStrength : 0.0f);
    postShader_->set("uInvResolution", glm::vec2(1.0f/(float)w_,1.0f/(float)h_));
    postShader_->set("uFXAAEnabled", (settings.fxaa && !settings.taa) ? 1 : 0);
    postShader_->set("uTAAEnabled", settings.taa && hasTaaHistory_ && hasPreviousViewProj_ ? 1 : 0);
    postShader_->set("uTAAHistoryWeight", glm::clamp(settings.taaHistoryWeight, 0.0f, 0.98f));
    postShader_->set("uMotionBlurEnabled", settings.motionBlur && hasPreviousViewProj_ ? 1 : 0);
    postShader_->set("uMotionBlurStrength", settings.motionBlurStrength);
    postShader_->set("uInvViewProj", glm::inverse(vp));
    postShader_->set("uPreviousViewProj", hasPreviousViewProj_ ? previousViewProj_ : vp);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Preserve the resolved color and matching depth for next-frame reprojection.
    glCopyImageSubData(ldrColor_, GL_TEXTURE_2D, 0, 0, 0, 0, historyColor_, GL_TEXTURE_2D, 0, 0, 0, 0, w_, h_, 1);
    glCopyImageSubData(gDepth_, GL_TEXTURE_2D, 0, 0, 0, 0, historyDepth_, GL_TEXTURE_2D, 0, 0, 0, 0, w_, h_, 1);
    hasTaaHistory_ = true;

    if (presentToScreen) {
        glBlitNamedFramebuffer(ldrFbo_, 0, 0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    previousViewProj_ = vp;
    hasPreviousViewProj_ = true;
}

} // namespace anvil
