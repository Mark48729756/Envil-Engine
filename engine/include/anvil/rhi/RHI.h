#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace anvil::rhi {

// New values are only ever appended, so serialized/stored enum values stay valid.
// Vulkan12 = compatibility tier (1.2 + dynamic rendering/sync2 extensions), Vulkan14 = target tier (core 1.4).
enum class Backend : std::uint8_t { OpenGL45, Vulkan12, Direct3D12, Vulkan14 };
enum class BufferUsage : std::uint8_t { Vertex, Index, Uniform, Storage, Readback, Indirect /* v3, appended */ };
enum class TextureFormat : std::uint8_t {
    R8, RG8, RGB8, RGBA8, SRGBA8, R16F, RG16F, RGBA16F, R32F, Depth24, Depth32F,
    BGRA8, SBGRA8, RGB10A2, R11G11B10F, Depth24Stencil8, Undefined,
    // v3 (appended): HDR/precision formats and block-compressed formats for imported assets (DDS / KTX2 / BCn)
    RG32F, RGBA32F, R16, RG16, RGBA16, R32UI, RGBA8Snorm,
    BC1, BC1_SRGB, BC2, BC2_SRGB, BC3, BC3_SRGB, BC4, BC4_SNORM, BC5, BC5_SNORM, BC6H_UF16, BC6H_SF16, BC7, BC7_SRGB };
// v3: shape of a texture resource.
enum class TextureDimension : std::uint8_t { Tex2D, Cube, Tex2DArray, CubeArray, Tex3D };
// Cube face order is the Vulkan / D3D12 / GL one: +X, -X, +Y, -Y, +Z, -Z (layer = arrayIndex * 6 + face).
enum class CubeFace : std::uint8_t { PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ };
enum class TextureFilter : std::uint8_t { Nearest, Linear, LinearMipmapLinear };
enum class TextureWrap : std::uint8_t { Repeat, ClampToEdge, MirroredRepeat, ClampToBorder };
enum class CompareOp : std::uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class CullMode : std::uint8_t { None, Back, Front };
enum class BlendMode : std::uint8_t { Alpha, Additive, Premultiplied };
enum class PrimitiveTopology : std::uint8_t { TriangleList, TriangleStrip, LineList, PointList };
enum class VertexFormat : std::uint8_t { Float, Float2, Float3, Float4, UByte4Norm, UInt };
enum class LoadOp : std::uint8_t { Load, Clear, DontCare };
enum class StoreOp : std::uint8_t { Store, DontCare };

enum class TextureUsage : std::uint8_t { Sampled = 1, RenderTarget = 2, DepthStencil = 4, Storage = 8, TransferSrc = 16, TransferDst = 32 };
constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) { return static_cast<TextureUsage>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)); }
constexpr bool hasUsage(TextureUsage v, TextureUsage f) { return (static_cast<std::uint8_t>(v) & static_cast<std::uint8_t>(f)) != 0; }

// ---- Binding model (identical for OpenGL, Vulkan and D3D12; shaders are written once in Vulkan-flavoured GLSL) ----
//   uniform buffers : GLSL `layout(set = 0, binding = N)`  N < kMaxUniformSlots   (HLSL b#)
//   textures        : GLSL `layout(set = 1, binding = N)`  N < kMaxTextureSlots   (HLSL t# + s#, combined image sampler)
//   per-draw data   : GLSL `layout(push_constant)`  <= kMaxPushConstantBytes     (Vulkan push constants / D3D12 root constants)
// Resource state transitions/barriers are tracked per texture by the backend; the renderer never records barriers by hand.
constexpr unsigned kMaxUniformSlots = 4;
constexpr unsigned kMaxTextureSlots = 16;
constexpr unsigned kMaxPushConstantBytes = 128;
constexpr unsigned kMaxColorAttachments = 8;
// v3 compute / storage binding model (same on every backend):
//   storage images  : GLSL `layout(set = 2, binding = N, rgba16f) uniform image2D`   N < kMaxStorageImageSlots   (HLSL u#)
//   storage buffers : GLSL `layout(set = 3, binding = N) buffer`                      N < kMaxStorageBufferSlots  (HLSL u# / SRV)
//   Sets 0 and 1 (UBO + sampled textures) are visible to compute too. Push constants are shared with graphics.
constexpr unsigned kMaxStorageImageSlots = 8;
constexpr unsigned kMaxStorageBufferSlots = 8;

struct DeviceCapabilities {
    Backend backend = Backend::OpenGL45;
    std::string deviceName;
    std::string apiVersion;
    int maxTextureSize = 0;
    int maxSamples = 1;
    bool debugOutput = false;
    bool directStateAccess = false;
    // v2
    std::uint32_t framesInFlight = 1;
    bool validationEnabled = false;
    bool discreteGpu = false;
    float maxAnisotropy = 1.0f;
    std::uint64_t dedicatedVideoMemory = 0;
    // v3
    bool compute = false;                 // compute pipelines + dispatch
    bool storageImages = false;
    bool cubeArrays = false;
    bool textureCompressionBC = false;    // BC1-BC7 sampled textures
    bool shaderOutputLayer = false;       // gl_Layer / SV_RenderTargetArrayIndex from the vertex stage (single-pass cubemap / array rendering)
    int maxArrayLayers = 1;
    int maxCubeSize = 0;
    int maxTexture3DSize = 0;
    std::uint32_t maxComputeWorkGroupSize[3] = {0, 0, 0};
    std::uint32_t maxComputeInvocations = 0;
};

// Backend creation options. Vulkan/D3D12 do not need a current GL context; `nativeWindow` is the GLFWwindow*
// used to pick a presentation-capable adapter/queue. All fields are optional.
struct DeviceCreateInfo {
    bool validation = false;          // Vulkan validation layer / D3D12 debug layer + GPU-based validation off
    int preferredAdapter = -1;        // index into the backend's adapter list; -1 = best (discrete, most VRAM)
    bool allowSoftwareAdapter = false;// WARP / lavapipe
    void* nativeWindow = nullptr;
    std::string appName = "ANVIL";
};

struct BufferDesc { std::size_t size = 0; BufferUsage usage = BufferUsage::Vertex; bool dynamic = false; std::string label; };
struct TextureDesc {
    int width = 1, height = 1, levels = 1; TextureFormat format = TextureFormat::RGBA8; std::string label;
    TextureUsage usage = TextureUsage::Sampled | TextureUsage::TransferDst;   // v2 (ignored by the OpenGL backend)
    int samples = 1;
    // v3. `layers` = array length (for Cube: 1 cube = 6 faces is implied, so `layers` counts cubes; for CubeArray it is the cube count).
    // Tex3D: `depth` is the volume depth. Defaults keep every existing 2D call site valid.
    TextureDimension dimension = TextureDimension::Tex2D;
    int layers = 1;
    int depth = 1;
};
// v3: one piece of initial data. `mip`/`layer` address the subresource (layer = arrayIndex*6+face for cubes); rowPitch/slicePitch 0 = tightly packed.
// For block-compressed formats the data is the compressed blocks of that mip.
struct TextureSubresourceData { const void* data = nullptr; std::size_t bytes = 0; std::uint32_t mip = 0, layer = 0, rowPitch = 0, slicePitch = 0; };
// v3: view of a texture. 0 counts mean "all remaining". `type` Auto = derive from the texture (and the sub-range: one layer of a cube -> 2D).
enum class ViewType : std::uint8_t { Auto, Tex2D, Tex2DArray, Cube, CubeArray, Tex3D };
struct TextureViewDesc {
    ViewType type = ViewType::Auto;
    std::uint32_t baseMip = 0, mipCount = 0, baseLayer = 0, layerCount = 0;
};
struct SamplerDesc {
    TextureFilter minFilter = TextureFilter::Linear; TextureFilter magFilter = TextureFilter::Linear;
    TextureWrap wrapU = TextureWrap::Repeat, wrapV = TextureWrap::Repeat;
    bool compare = false; CompareOp compareOp = CompareOp::LessEqual;   // v2: hardware PCF shadow sampler
    float maxAnisotropy = 1.0f; bool whiteBorder = false;                // v2: ClampToBorder colour (opaque white / transparent black)
};
// GL backend: GLSL source. Vulkan/D3D12: precompiled blobs (SPIR-V / DXIL) produced by cmake/AnvilShaders.cmake.
struct ShaderDesc {
    std::string vertexSource, fragmentSource, label;
    std::vector<std::uint32_t> vertexSpirv, fragmentSpirv;   // v2
    std::vector<std::uint8_t> vertexDxil, fragmentDxil;      // v2
    // v3 compute stage (a Shader holds either vertex+fragment or a compute stage)
    std::string computeSource;
    std::vector<std::uint32_t> computeSpirv;
    std::vector<std::uint8_t> computeDxil;
};
struct VertexBinding { std::uint32_t binding = 0, stride = 0; bool perInstance = false; };
struct VertexAttribute { std::uint32_t location = 0, binding = 0, offset = 0; VertexFormat format = VertexFormat::Float3; };
struct VertexLayout { std::vector<VertexBinding> bindings; std::vector<VertexAttribute> attributes; };
struct PipelineDesc {
    bool depthTest = true; bool depthWrite = true; bool blend = false; bool cull = true;
    // v2 (Vulkan/D3D12 bake all of this into the pipeline state object)
    CompareOp depthCompare = CompareOp::LessEqual;
    BlendMode blendMode = BlendMode::Alpha;
    CullMode cullMode = CullMode::Back;              // used when `cull` is true
    bool frontFaceCCW = true;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    VertexLayout vertexLayout;                       // empty = vertex-pulling / fullscreen triangle (SV_VertexID)
    std::vector<TextureFormat> colorFormats;         // must match the render pass this pipeline is used in
    TextureFormat depthFormat = TextureFormat::Undefined;
    float depthBiasConstant = 0.0f, depthBiasSlope = 0.0f;
    std::uint32_t pushConstantBytes = 0;             // <= kMaxPushConstantBytes
    int samples = 1;
    std::string label;
};

// v3: compute pipeline. Local workgroup size comes from the shader (`local_size_x/y/z`); it is reported back for dispatch maths.
struct ComputePipelineDesc { std::uint32_t pushConstantBytes = 0; std::string label; };
class Buffer { public: virtual ~Buffer() = default; virtual std::size_t size() const noexcept = 0; virtual void update(std::size_t offset, const void* data, std::size_t bytes) = 0; };
class Texture {
public:
    virtual ~Texture() = default; virtual int width() const noexcept = 0; virtual int height() const noexcept = 0;
    virtual TextureFormat format() const noexcept { return TextureFormat::RGBA8; }     // v2: Vulkan/D3D12 override
    // Opaque native handle for UI backends (ImGui): GL texture id / VkImageView / D3D12 SRV GPU handle. 0 if none.
    virtual std::uintptr_t nativeHandle() const noexcept { return 0; }
    // v3: Vulkan/D3D12 override; the defaults describe a plain mip-less 2D texture.
    virtual TextureDimension dimension() const noexcept { return TextureDimension::Tex2D; }
    virtual int levels() const noexcept { return 1; }
    virtual int layers() const noexcept { return 1; }          // array layers as stored (cube = 6 per cube)
    virtual int depth() const noexcept { return 1; }
};
class Sampler { public: virtual ~Sampler() = default; };
class Shader { public: virtual ~Shader() = default; };
class Pipeline { public: virtual ~Pipeline() = default; };
class ComputePipeline { public: virtual ~ComputePipeline() = default; virtual void localSize(std::uint32_t out[3]) const noexcept { out[0] = out[1] = out[2] = 1; } };

struct ColorAttachment { std::shared_ptr<Texture> texture; LoadOp load = LoadOp::Clear; StoreOp store = StoreOp::Store; float clear[4] = {0, 0, 0, 0}; std::uint32_t mip = 0;
                         std::uint32_t layer = 0, layerCount = 1; };   // v3: render into one layer / cube face (layerCount > 1 needs caps.shaderOutputLayer)
struct DepthAttachment { std::shared_ptr<Texture> texture; LoadOp load = LoadOp::Clear; StoreOp store = StoreOp::Store; float clearDepth = 1.0f;
                         std::uint32_t mip = 0, layer = 0, layerCount = 1; };   // v3
struct RenderPassDesc { std::vector<ColorAttachment> colors; DepthAttachment depth; std::string label; };

// v3: GPU timing. Every CommandList::pushDebugGroup/popDebugGroup pair - and every labelled render pass - is one timed scope.
// Fixed-size POD so resolving timings never allocates. Scopes are listed in submission order; `depth` is the nesting level.
constexpr unsigned kGpuScopeNameLength = 40;
struct GpuScopeTiming { char name[kGpuScopeNameLength] = {}; std::uint32_t depth = 0; float ms = 0.0f; };

struct UnsupportedByBackend : std::logic_error { using std::logic_error::logic_error; };

class CommandList {
public:
    virtual ~CommandList() = default;
    virtual void begin() = 0; virtual void end() = 0;
    virtual void setPipeline(const std::shared_ptr<Pipeline>& pipeline) = 0;
    virtual void bindVertexBuffer(const std::shared_ptr<Buffer>& buffer, unsigned binding = 0) = 0;
    virtual void bindIndexBuffer(const std::shared_ptr<Buffer>& buffer) = 0;   // 32-bit indices
    virtual void bindTexture(unsigned unit, const std::shared_ptr<Texture>& texture, const std::shared_ptr<Sampler>& sampler = {}) = 0;
    virtual void draw(std::uint32_t vertexCount, std::uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex = 0) = 0;

    // ---- v2: explicit-API surface. The OpenGL backend keeps the throwing defaults (its renderer still talks to GL directly). ----
    virtual void beginRenderPass(const RenderPassDesc&) { unsupported("beginRenderPass"); }
    virtual void endRenderPass() { unsupported("endRenderPass"); }
    virtual void setViewport(float, float, float, float, float = 0.0f, float = 1.0f) { unsupported("setViewport"); }
    virtual void setScissor(int, int, std::uint32_t, std::uint32_t) { unsupported("setScissor"); }
    virtual void bindUniformBuffer(unsigned, const std::shared_ptr<Buffer>&, std::size_t = 0, std::size_t = 0) { unsupported("bindUniformBuffer"); }
    virtual void pushConstants(const void*, std::uint32_t, std::uint32_t = 0) { unsupported("pushConstants"); }
    virtual void drawInstanced(std::uint32_t, std::uint32_t, std::uint32_t = 0, std::uint32_t = 0) { unsupported("drawInstanced"); }
    virtual void drawIndexedInstanced(std::uint32_t, std::uint32_t, std::uint32_t = 0, std::int32_t = 0, std::uint32_t = 0) { unsupported("drawIndexedInstanced"); }
    virtual void generateMipmaps(const std::shared_ptr<Texture>&) { unsupported("generateMipmaps"); }
    // ---- v3: views, storage resources, compute. Layout/barrier tracking is per subresource (mip x layer) inside the backend. ----
    // Sampled view of part of a texture (cube face, one mip, array slice...). The binding is reset by begin().
    virtual void bindTextureView(unsigned, const std::shared_ptr<Texture>&, const TextureViewDesc&, const std::shared_ptr<Sampler>& = {}) { unsupported("bindTextureView"); }
    // bindStorageTexture: needs TextureUsage::Storage. Tex3D -> whole 3D view of `mip`; layerCount == 1 -> image2D of that layer;
    // layerCount > 1 (or 0 = all remaining) -> image2DArray (a cube is 6 layers). The image is moved to GENERAL by dispatch().
    // bindStorageBuffer: BufferUsage::Storage / Indirect; `dynamic` buffers are CPU-updated rings (read-only in shaders). nullptr unbinds a slot.
    // A texture may not be bound sampled and as storage with overlapping mips/layers in the same dispatch (throws std::logic_error).
    virtual void bindStorageTexture(unsigned, const std::shared_ptr<Texture>&, std::uint32_t = 0 /*mip*/, std::uint32_t = 0 /*baseLayer*/, std::uint32_t = 1 /*layerCount*/) { unsupported("bindStorageTexture"); }
    virtual void bindStorageBuffer(unsigned, const std::shared_ptr<Buffer>&, std::size_t = 0, std::size_t = 0) { unsupported("bindStorageBuffer"); }
    virtual void setComputePipeline(const std::shared_ptr<ComputePipeline>&) { unsupported("setComputePipeline"); }
    virtual void dispatch(std::uint32_t, std::uint32_t = 1, std::uint32_t = 1) { unsupported("dispatch"); }
    virtual void dispatchIndirect(const std::shared_ptr<Buffer>&, std::size_t = 0) { unsupported("dispatchIndirect"); }
    // Copies (outside a render pass). Same format; dstLayer/srcLayer address cube faces / array slices.
    virtual void copyTexture(const std::shared_ptr<Texture>&, std::uint32_t /*srcMip*/, std::uint32_t /*srcLayer*/,
                             const std::shared_ptr<Texture>&, std::uint32_t /*dstMip*/, std::uint32_t /*dstLayer*/) { unsupported("copyTexture"); }
    virtual void generateMipmapsRange(const std::shared_ptr<Texture>&, std::uint32_t /*baseLayer*/, std::uint32_t /*layerCount*/) { unsupported("generateMipmapsRange"); }
    virtual void pushDebugGroup(const char*) {}
    virtual void popDebugGroup() {}
private:
    static void unsupported(const char* what) { throw UnsupportedByBackend(std::string("CommandList::") + what + " is not supported by this RHI backend"); }
};

struct SwapchainDesc { void* nativeWindow = nullptr; int width = 0, height = 0; bool vsync = true; std::uint32_t imageCount = 3; bool hdr10 = false; };
class Swapchain {
public:
    virtual ~Swapchain() = default;
    virtual void resize(int width, int height) = 0;
    virtual TextureFormat format() const noexcept = 0;
    virtual int width() const noexcept = 0; virtual int height() const noexcept = 0;
    // Returns the backbuffer to render into this frame, or null if the surface is minimised / out of date (skip the frame).
    virtual std::shared_ptr<Texture> acquire() = 0;
    virtual void present() = 0;
};

class Device {
public:
    virtual ~Device() = default;
    virtual const DeviceCapabilities& capabilities() const noexcept = 0;
    virtual std::shared_ptr<Buffer> createBuffer(const BufferDesc&, const void* initialData = nullptr) = 0;
    virtual std::shared_ptr<Texture> createTexture2D(const TextureDesc&, const void* initialData = nullptr) = 0;
    virtual std::shared_ptr<Sampler> createSampler(const SamplerDesc&) = 0;
    virtual std::shared_ptr<Shader> createShader(const ShaderDesc&) = 0;
    virtual std::shared_ptr<Pipeline> createPipeline(const std::shared_ptr<Shader>&, const PipelineDesc&) = 0;
    virtual std::unique_ptr<CommandList> createCommandList() = 0;

    // ---- v3: cubemaps, arrays, volumes, compute ----
    // General texture creation. `data` holds `dataCount` subresources (missing ones stay undefined); a single {mip 0, layer 0} entry plus
    // levels > 1 uploads level 0 and generates the rest (not for block-compressed formats).
    virtual std::shared_ptr<Texture> createTexture(const TextureDesc&, const TextureSubresourceData* /*data*/ = nullptr, std::size_t /*dataCount*/ = 0) {
        throw UnsupportedByBackend("Device::createTexture is not supported by this RHI backend"); }
    virtual std::shared_ptr<ComputePipeline> createComputePipeline(const std::shared_ptr<Shader>&, const ComputePipelineDesc&) {
        throw UnsupportedByBackend("Device::createComputePipeline is not supported by this RHI backend"); }
    // True if the format can be sampled on this device (BCn depends on the GPU); a render/storage query follows with the usage bits.
    virtual bool supportsFormat(TextureFormat, TextureUsage = TextureUsage::Sampled) const noexcept { return false; }

    // ---- v2 frame loop:  beginFrame -> swapchain.acquire -> record -> submit -> swapchain.present -> endFrame ----
    virtual std::unique_ptr<Swapchain> createSwapchain(const SwapchainDesc&) { throw UnsupportedByBackend("Device::createSwapchain is not supported by this RHI backend"); }
    virtual void beginFrame() {}                       // waits for the frame slot's fence, recycles per-frame upload memory
    virtual void submit(CommandList&) { throw UnsupportedByBackend("Device::submit is not supported by this RHI backend"); }
    virtual void endFrame() {}
    virtual void waitIdle() {}

    // ---- v3: GPU timestamp profiling (Vulkan today; OpenGL/D3D12 report false and an empty list) ----
    virtual bool supportsGpuTimings() const noexcept { return false; }
    // Newest frame whose queries have been read back (latency = framesInFlight). The reference stays valid until the next beginFrame().
    virtual const std::vector<GpuScopeTiming>& gpuTimings() const noexcept { static const std::vector<GpuScopeTiming> none; return none; }
    virtual std::uint64_t gpuTimingsFrame() const noexcept { return 0; }   // increases every time gpuTimings() holds a new frame
};

std::unique_ptr<Device> createOpenGL45Device();
// Defined only when the matching backend is compiled in (ANVIL_WITH_VULKAN / ANVIL_WITH_D3D12).
std::unique_ptr<Device> createVulkanDevice(const DeviceCreateInfo&, Backend tier);
std::unique_ptr<Device> createD3D12Device(const DeviceCreateInfo&);

// The selector fails explicitly for backends that are not compiled into this build.
std::unique_ptr<Device> createDevice(Backend backend, const DeviceCreateInfo& info = {});
bool isBackendAvailable(Backend backend) noexcept;
const char* backendName(Backend backend) noexcept;
// "gl" / "opengl", "vulkan" / "vk" / "vulkan14", "vulkan12", "d3d12" / "dx12" / "directx12" (case-insensitive). False if unknown.
bool parseBackend(const char* text, Backend& out) noexcept;
// Reads ANVIL_BACKEND; returns `fallback` if unset, unknown, or not compiled in.
Backend backendFromEnvironment(Backend fallback = Backend::OpenGL45) noexcept;

} // namespace anvil::rhi
