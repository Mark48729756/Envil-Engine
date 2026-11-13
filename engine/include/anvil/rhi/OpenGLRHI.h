#pragma once
#include "anvil/rhi/RHI.h"
#include <glad/gl.h>
#include <array>
#include <vector>

namespace anvil::rhi::gl {
class Buffer final : public rhi::Buffer {
public: Buffer(const BufferDesc&, const void*); ~Buffer() override; std::size_t size() const noexcept override { return size_; }
    void update(std::size_t, const void*, std::size_t) override; GLuint id() const noexcept { return id_; }
    BufferUsage usage() const noexcept { return usage_; }
private: GLuint id_ = 0; std::size_t size_ = 0; GLenum target_ = GL_ARRAY_BUFFER; bool dynamic_ = false; BufferUsage usage_ = BufferUsage::Vertex;
};
class Texture final : public rhi::Texture {
public: Texture(const TextureDesc&, const void*); ~Texture() override; int width() const noexcept override { return width_; } int height() const noexcept override { return height_; } GLuint id() const noexcept { return id_; }
    TextureFormat format() const noexcept override { return format_; } int levels() const noexcept override { return levels_; }
    TextureUsage usage() const noexcept { return usage_; } GLenum internalFormat() const noexcept;
private: GLuint id_ = 0; int width_ = 1, height_ = 1; TextureFormat format_ = TextureFormat::RGBA8; TextureUsage usage_ = TextureUsage::Sampled; int levels_ = 1;
};
class Sampler final : public rhi::Sampler { public: explicit Sampler(const SamplerDesc&); ~Sampler() override; GLuint id() const noexcept { return id_; } private: GLuint id_ = 0; };
class Shader final : public rhi::Shader { public: explicit Shader(const ShaderDesc&); ~Shader() override; GLuint id() const noexcept { return id_; } bool isCompute() const noexcept { return compute_; } private: void buildCompute(const ShaderDesc&); GLuint id_ = 0; bool compute_ = false; };
class Pipeline final : public rhi::Pipeline { public: Pipeline(const std::shared_ptr<rhi::Shader>&, const PipelineDesc&); GLuint program() const noexcept { return program_; } const PipelineDesc& desc() const noexcept { return desc_; } private: std::shared_ptr<rhi::Shader> shader_; GLuint program_ = 0; PipelineDesc desc_{}; };
// v3 compute (OpenGL 4.3+). The GL backend takes GLSL source, so compute shaders are written in GL flavour (`#version 430 core`) with flat
// binding points instead of Vulkan sets: `layout(std140, binding = N) uniform` (UBO), `layout(binding = N) uniform sampler2D` (texture unit N),
// `layout(binding = N, rgba16f) uniform image2D` (storage image N), `layout(std430, binding = N) buffer` (storage buffer N). Same N ranges as RHI.h.
// Not supported on GL: compute push constants (use a uniform buffer), non-2D storage images / textures, bindTextureView, copyTexture.
class ComputePipeline final : public rhi::ComputePipeline {
public: ComputePipeline(const std::shared_ptr<rhi::Shader>&, const ComputePipelineDesc&); GLuint program() const noexcept { return program_; }
    void localSize(std::uint32_t out[3]) const noexcept override { out[0] = local_[0]; out[1] = local_[1]; out[2] = local_[2]; }
private: std::shared_ptr<Shader> shader_; GLuint program_ = 0; std::uint32_t local_[3] = {1, 1, 1};
};
class CommandList final : public rhi::CommandList {
public: CommandList(); void begin() override; void end() override; void setPipeline(const std::shared_ptr<rhi::Pipeline>&) override;
    void bindVertexBuffer(const std::shared_ptr<rhi::Buffer>&, unsigned) override; void bindIndexBuffer(const std::shared_ptr<rhi::Buffer>&) override;
    void bindTexture(unsigned, const std::shared_ptr<rhi::Texture>&, const std::shared_ptr<rhi::Sampler>&) override;
    void draw(std::uint32_t, std::uint32_t) override; void drawIndexed(std::uint32_t, std::uint32_t) override;
    // v3 compute
    void bindStorageTexture(unsigned, const std::shared_ptr<rhi::Texture>&, std::uint32_t, std::uint32_t, std::uint32_t) override;
    void bindStorageBuffer(unsigned, const std::shared_ptr<rhi::Buffer>&, std::size_t, std::size_t) override;
    void setComputePipeline(const std::shared_ptr<rhi::ComputePipeline>&) override;
    void dispatch(std::uint32_t, std::uint32_t, std::uint32_t) override;
    void dispatchIndirect(const std::shared_ptr<rhi::Buffer>&, std::size_t) override;
private:
    struct StorBuf { std::shared_ptr<Buffer> buf; std::size_t offset = 0, range = 0; };
    struct StorImg { std::shared_ptr<Texture> tex; std::uint32_t mip = 0; };
    // Hazard tracking, same model as the Vulkan backend: accesses since the last barrier; a barrier only before an access that overlaps an
    // earlier one when at least one of them writes. GLSL has no reflection here, so every storage buffer / image counts as written;
    // sampled textures and the indirect argument buffer count as reads.
    struct BufAccess { GLuint buf; std::size_t b, e; bool write; };
    struct ImgAccess { GLuint tex; std::uint32_t m0, mc; bool write; };
    void requireRecording() const; void requireCompute() const;
    void useGraphicsProgram();
    void prepareDispatch(const std::shared_ptr<Buffer>& indirect, std::size_t indirectOffset);
    void trackAccesses(const std::shared_ptr<Buffer>& indirect, std::size_t indirectOffset);
    void flushAccesses(bool endOfList);
    std::shared_ptr<Pipeline> pipeline_; std::shared_ptr<ComputePipeline> cpipeline_; bool recording_ = false; GLuint program_ = 0;
    std::array<StorBuf, kMaxStorageBufferSlots> sbuf_{}; std::array<StorImg, kMaxStorageImageSlots> simg_{}; std::array<std::shared_ptr<Texture>, kMaxTextureSlots> tex_{};
    std::vector<BufAccess> openBuf_; std::vector<ImgAccess> openImg_; bool openWrite_ = false;
    bool computeOk_ = false; GLint maxCount_[3] = {0, 0, 0}; GLint ssboAlign_ = 256; GLint maxSsboRange_ = 0;
};
class Device final : public rhi::Device {
public: Device(); const DeviceCapabilities& capabilities() const noexcept override { return caps_; }
    std::shared_ptr<rhi::Buffer> createBuffer(const BufferDesc&, const void*) override;
    std::shared_ptr<rhi::Texture> createTexture2D(const TextureDesc&, const void*) override;
    std::shared_ptr<rhi::Sampler> createSampler(const SamplerDesc&) override;
    std::shared_ptr<rhi::Shader> createShader(const ShaderDesc&) override;
    std::shared_ptr<rhi::Pipeline> createPipeline(const std::shared_ptr<rhi::Shader>&, const PipelineDesc&) override;
    std::unique_ptr<rhi::CommandList> createCommandList() override;
    std::shared_ptr<rhi::ComputePipeline> createComputePipeline(const std::shared_ptr<rhi::Shader>&, const ComputePipelineDesc&) override;
private: DeviceCapabilities caps_{};
};
}
