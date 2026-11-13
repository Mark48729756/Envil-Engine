// ANVIL RHI - Vulkan backend (M34).
//
// Tiers: Backend::Vulkan14 requires a Vulkan 1.4 instance + device; Backend::Vulkan12 runs on 1.2+ devices using
// VK_KHR_dynamic_rendering / VK_KHR_synchronization2. Both share one code path (entry points are resolved with
// vkGetDeviceProcAddr, core name first, KHR name second).
//
// Design notes
//  * Dynamic rendering only (no VkRenderPass / VkFramebuffer). Sync2 barriers, timeline semaphore for frame pacing.
//  * Image layouts are tracked per texture inside the backend and barriers are emitted automatically, in RECORD order.
//    Command lists must therefore be submitted in the order they were recorded.
//  * Descriptors: set 0 = 4 uniform buffers, set 1 = 16 combined image samplers, set 2 = 8 storage images (compute only),
//    set 3 = 8 storage buffers (compute only). Sets are allocated from per-frame pools (reset in beginFrame). Unbound slots of
//    sets 0/1/3 point at a fallback buffer / 1x1 white texture; unbound storage image slots stay unwritten (a shader must not touch them).
//  * v3 textures: 2D, cube, 2D array, cube array and 3D images, BCn and extra float/16-bit formats. Layouts are tracked per subresource
//    (mip x layer, collapsed to one value while they agree), so cube faces / array slices / mips can be rendered to and sampled independently.
//  * One fixed pipeline layout shared by graphics and compute (sets 0..3, 128 bytes of push constants for vertex | fragment | compute),
//    so descriptor sets and push constants survive pipeline changes. Devices that cannot provide sets 2/3 (caps.compute == false)
//    get a 2-set layout and compute calls throw UnsupportedByBackend.
//  * v3 compute (M43 step 4): ComputePipeline (SPIR-V, local_size read from the module), dispatch / dispatchIndirect, bindStorageTexture
//    (image goes to GENERAL, per-subresource, one batched barrier per dispatch) and bindStorageBuffer. Storage-buffer hazards are
//    tracked per buffer range (RAW / WAR / WAW, using the NonWritable decorations of the SPIR-V): a barrier is emitted only before a
//    dispatch that really conflicts with an earlier access, before the next render pass (writes only) and at the end of the command
//    list. Independent dispatches (disjoint ranges, read-only sharing) overlap freely.
//  * Viewport is flipped (negative height) so GL-style clip space and winding work unchanged. Texture origin is
//    top-left (Vulkan convention).
//  * `dynamic` buffers keep one copy per frame in flight (+ a CPU shadow): update() is safe while the GPU still
//    reads the previous frame. Static buffers are updated through a staging copy that waits for the queue (slow path).
//  * Resource destruction is deferred until the GPU has finished the last submit made before the destruction.
//  * Single-threaded: create resources, record and submit from one thread.
//  * Only one command list per frame may render to the swapchain backbuffer.
#include "anvil/rhi/RHI.h"
#include "anvil/Core.h"
#include "rhi/SpirvReflect.h"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define VMA_VULKAN_VERSION 1003000
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <tuple>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef VK_API_VERSION_1_4
#define VK_API_VERSION_1_4 VK_MAKE_API_VERSION(0, 1, 4, 0)
#endif

namespace anvil::rhi::vk {
namespace {

constexpr std::uint32_t kFrames = 2;
constexpr VkDeviceSize kFallbackUboSize = 16384;
constexpr VkShaderStageFlags kPushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
constexpr VkShaderStageFlags kSetStages = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;   // sets 0 and 1 are visible to compute too

std::string vkErr(VkResult r) {
    switch (r) {
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        default: return "VkResult(" + std::to_string(static_cast<int>(r)) + ")";
    }
}
#define VK_CHECK(x) do { const VkResult vkr_ = (x); if (vkr_ != VK_SUCCESS) \
    throw std::runtime_error(std::string("Vulkan call failed: " #x " -> ") + ::anvil::rhi::vk::vkErr(vkr_)); } while (0)

// ---------------------------------------------------------------------------------------------------------------------
// format helpers
bool isDepthFormat(TextureFormat f) { return f == TextureFormat::Depth24 || f == TextureFormat::Depth32F || f == TextureFormat::Depth24Stencil8; }
bool hasStencil(TextureFormat f) { return f == TextureFormat::Depth24Stencil8; }
bool isCompressed(TextureFormat f) { return f >= TextureFormat::BC1 && f <= TextureFormat::BC7_SRGB; }   // BC1..BC7_SRGB are contiguous in the enum
std::uint32_t blockBytes(TextureFormat f) {   // 4x4 block size in bytes
    switch (f) {
        case TextureFormat::BC1: case TextureFormat::BC1_SRGB: case TextureFormat::BC4: case TextureFormat::BC4_SNORM: return 8;
        default: return 16;
    }
}
std::uint32_t bytesPerPixel(TextureFormat f) {   // 0 for block-compressed formats (use blockBytes)
    switch (f) {
        case TextureFormat::R8: return 1;
        case TextureFormat::RG8: case TextureFormat::R16F: case TextureFormat::R16: return 2;
        case TextureFormat::RGB8: return 3;   // expanded to RGBA8 on upload
        case TextureFormat::RGBA16F: case TextureFormat::RG32F: case TextureFormat::RGBA16: return 8;
        case TextureFormat::RGBA32F: return 16;
        case TextureFormat::Undefined: return 0;
        default: return isCompressed(f) ? 0 : 4;
    }
}
VkImageAspectFlags attachAspect(TextureFormat f) {
    if (!isDepthFormat(f)) return VK_IMAGE_ASPECT_COLOR_BIT;
    return VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(f) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
}
VkImageAspectFlags sampleAspect(TextureFormat f) { return isDepthFormat(f) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT; }
VkImageLayout readLayout(TextureFormat f) { return isDepthFormat(f) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }

VkCompareOp toVk(CompareOp o) {
    switch (o) {
        case CompareOp::Never: return VK_COMPARE_OP_NEVER; case CompareOp::Less: return VK_COMPARE_OP_LESS;
        case CompareOp::Equal: return VK_COMPARE_OP_EQUAL; case CompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater: return VK_COMPARE_OP_GREATER; case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL; case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_ALWAYS;
}
VkSamplerAddressMode toVk(TextureWrap w) {
    switch (w) {
        case TextureWrap::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrap::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureWrap::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}
VkFormat toVk(VertexFormat f) {
    switch (f) {
        case VertexFormat::Float: return VK_FORMAT_R32_SFLOAT; case VertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT; case VertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::UByte4Norm: return VK_FORMAT_R8G8B8A8_UNORM; case VertexFormat::UInt: return VK_FORMAT_R32_UINT;
    }
    return VK_FORMAT_R32G32B32_SFLOAT;
}
VkPrimitiveTopology toVk(PrimitiveTopology t) {
    switch (t) {
        case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}
TextureFormat fromVkSurfaceFormat(VkFormat f) {
    switch (f) {
        case VK_FORMAT_B8G8R8A8_UNORM: return TextureFormat::BGRA8;
        case VK_FORMAT_B8G8R8A8_SRGB: return TextureFormat::SBGRA8;
        case VK_FORMAT_R8G8B8A8_UNORM: return TextureFormat::RGBA8;
        case VK_FORMAT_R8G8B8A8_SRGB: return TextureFormat::SRGBA8;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return TextureFormat::RGB10A2;
        default: return TextureFormat::BGRA8;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// barrier helpers (sync2)
struct Scope { VkPipelineStageFlags2 stage; VkAccessFlags2 access; };
Scope scopeFor(VkImageLayout l) {
    switch (l) {
        case VK_IMAGE_LAYOUT_UNDEFINED: return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0};
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return {VK_PIPELINE_STAGE_2_NONE, 0};
        default: return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
    }
}
VkImageMemoryBarrier2 makeBarrier(VkImage img, VkImageAspectFlags aspect, std::uint32_t baseMip, std::uint32_t mips,
                                  VkImageLayout oldL, VkImageLayout newL, Scope src, Scope dst,
                                  std::uint32_t baseLayer = 0, std::uint32_t layers = VK_REMAINING_ARRAY_LAYERS) {
    VkImageMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b.srcStageMask = src.stage; b.srcAccessMask = src.access;
    b.dstStageMask = dst.stage; b.dstAccessMask = dst.access;
    b.oldLayout = oldL; b.newLayout = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {aspect, baseMip, mips, baseLayer, layers};
    return b;
}

// ---------------------------------------------------------------------------------------------------------------------
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* d, void*) {
    if (sev >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        ANVIL_LOG("Vulkan %s: %s", sev >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "ERROR" : "warning", d->pMessage);
    return VK_FALSE;
}

std::uint64_t alignUp(std::uint64_t v, std::uint64_t a) { return (v + a - 1) / a * a; }

} // namespace

class Swapchain;

// ---------------------------------------------------------------------------------------------------------------------
// Core: instance, device, allocator, frame timeline, descriptor pools, deferred destruction. Shared by every resource so
// that resources can safely outlive the Device object.
struct Core {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VmaAllocator vma = nullptr;
    Backend tier = Backend::Vulkan14;
    std::uint32_t deviceApi = 0;
    VkPhysicalDeviceProperties props{};
    DeviceCapabilities caps;
    bool anisotropy = false;
    bool bc = false, cubeArray = false, shaderOutLayer = false;   // v3 device features that were enabled
    bool computeOk = false;                                       // v3 compute: sets 2/3 exist, limits and queue allow compute
    bool storageExtFormats = false, storageNoFormat = false;      // shaderStorageImageExtendedFormats / Read+WriteWithoutFormat enabled

    PFN_vkCmdBeginRenderingKHR cmdBeginRendering = nullptr;
    PFN_vkCmdEndRenderingKHR cmdEndRendering = nullptr;
    PFN_vkCmdPipelineBarrier2KHR cmdBarrier2 = nullptr;
    PFN_vkQueueSubmit2KHR queueSubmit2 = nullptr;
    PFN_vkCmdWriteTimestamp2KHR cmdWriteTimestamp2 = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT cmdBeginLabel = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmdEndLabel = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger = nullptr;

    VkSemaphore timeline = VK_NULL_HANDLE;
    std::uint64_t submitted = 0;
    std::uint64_t frameValue[kFrames] = {};
    std::uint32_t frame = 0;

    VkDescriptorSetLayout uboLayout = VK_NULL_HANDLE, texLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout storImgLayout = VK_NULL_HANDLE, storBufLayout = VK_NULL_HANDLE;   // sets 2 / 3 (only when computeOk)
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkSampler defaultSampler = VK_NULL_HANDLE;
    VkCommandPool immPool = VK_NULL_HANDLE;

    VkImage fbImage = VK_NULL_HANDLE; VkImageView fbView = VK_NULL_HANDLE; VmaAllocation fbAlloc = nullptr;
    VkBuffer fbBuffer = VK_NULL_HANDLE; VmaAllocation fbBufAlloc = nullptr;

    std::vector<VkDescriptorPool> pools[kFrames];
    std::uint32_t poolIndex[kFrames] = {};

    VkFormat d24 = VK_FORMAT_D32_SFLOAT, d24s8 = VK_FORMAT_D32_SFLOAT_S8_UINT;

    // ---- GPU timestamp profiling: one query pool per frame slot, fixed-size scope tables (no allocation while recording) ----
    static constexpr std::uint32_t kMaxTimedScopes = 96;
    struct TimedScope { char name[kGpuScopeNameLength]; std::uint32_t depth, q0, q1; };
    VkQueryPool queryPool[kFrames] = {};
    bool timestamps = false;
    double tsNsPerTick = 0.0;
    std::uint64_t tsMask = ~0ull;
    std::array<TimedScope, kMaxTimedScopes> scopes[kFrames];
    std::uint32_t scopeCount[kFrames] = {};
    std::uint32_t queryCount[kFrames] = {};
    bool queryReset[kFrames] = {};   // true once this slot's pool reset has been recorded for the current frame
    std::array<std::uint32_t, 32> openScopes[kFrames] = {};   // index into scopes[], kNoScope = overflowed (keeps push/pop balanced)
    std::uint32_t openDepth[kFrames] = {};
    static constexpr std::uint32_t kNoScope = 0xFFFFFFFFu;
    std::vector<GpuScopeTiming> resolved;
    std::uint64_t resolvedSerial = 0;
    void resolveTimings(std::uint32_t slot);

    struct Retired { std::uint64_t value; std::function<void()> fn; };
    std::deque<Retired> retired;
    std::mutex mu;

    Core() = default;
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    ~Core();

    void init(const DeviceCreateInfo& info, Backend tierIn);
    VkFormat vkFormat(TextureFormat f) const;
    bool formatUsable(TextureFormat f, TextureUsage u) const;
    void retire(std::function<void()> fn);
    void collect(bool all = false);
    void beginFrame();
    VkDescriptorSet allocSet(VkDescriptorSetLayout layout);
    VkDescriptorPool makePool();

    template <class F> void immediate(F&& f) {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = immPool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        VkCommandBuffer cmd{};
        VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
        f(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd));
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));
        vkFreeCommandBuffers(device, immPool, 1, &cmd);
    }

    struct Staging { VkBuffer buf = VK_NULL_HANDLE; VmaAllocation alloc = nullptr; };
    Staging makeStaging(const void* data, std::size_t bytes) {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = bytes; bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_AUTO;
        ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        Staging s; VmaAllocationInfo info{};
        VK_CHECK(vmaCreateBuffer(vma, &bi, &ai, &s.buf, &s.alloc, &info));
        std::memcpy(info.pMappedData, data, bytes);
        vmaFlushAllocation(vma, s.alloc, 0, VK_WHOLE_SIZE);
        return s;
    }
    void destroyStaging(Staging& s) { if (s.buf) vmaDestroyBuffer(vma, s.buf, s.alloc); s = {}; }

    void uploadBuffer(VkBuffer dst, VkDeviceSize offset, const void* data, std::size_t bytes) {
        Staging s = makeStaging(data, bytes);
        immediate([&](VkCommandBuffer cmd) { VkBufferCopy r{0, offset, bytes}; vkCmdCopyBuffer(cmd, s.buf, dst, 1, &r); });
        destroyStaging(s);
    }

    void barriers(VkCommandBuffer cmd, const VkImageMemoryBarrier2* b, std::uint32_t n) {
        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.imageMemoryBarrierCount = n; dep.pImageMemoryBarriers = b;
        cmdBarrier2(cmd, &dep);
    }
    void recordMips(VkCommandBuffer cmd, VkImage img, int w, int h, int levels, VkImageLayout level0Old, int layers = 1, int depth = 1) {
        const auto A = VK_IMAGE_ASPECT_COLOR_BIT;
        {
            VkImageMemoryBarrier2 b[2] = {
                makeBarrier(img, A, 0, 1, level0Old, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, scopeFor(level0Old), scopeFor(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)),
                makeBarrier(img, A, 1, static_cast<std::uint32_t>(levels - 1), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            scopeFor(VK_IMAGE_LAYOUT_UNDEFINED), scopeFor(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))};
            barriers(cmd, b, 2);
        }
        int mw = w, mh = h, md = depth;
        for (int i = 1; i < levels; ++i) {
            const int nw = std::max(1, mw / 2), nh = std::max(1, mh / 2), nd = std::max(1, md / 2);
            VkImageBlit blit{};
            blit.srcSubresource = {A, static_cast<std::uint32_t>(i - 1), 0, static_cast<std::uint32_t>(layers)};
            blit.srcOffsets[1] = {mw, mh, md};
            blit.dstSubresource = {A, static_cast<std::uint32_t>(i), 0, static_cast<std::uint32_t>(layers)};
            blit.dstOffsets[1] = {nw, nh, nd};
            vkCmdBlitImage(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            VkImageMemoryBarrier2 b = makeBarrier(img, A, static_cast<std::uint32_t>(i), 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, scopeFor(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), scopeFor(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
            barriers(cmd, &b, 1);
            mw = nw; mh = nh; md = nd;
        }
        VkImageMemoryBarrier2 b = makeBarrier(img, A, 0, static_cast<std::uint32_t>(levels), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, scopeFor(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL), scopeFor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        barriers(cmd, &b, 1);
    }
};

VkFormat Core::vkFormat(TextureFormat f) const {
    switch (f) {
        case TextureFormat::R8: return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8: return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8: case TextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::SRGBA8: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::R16F: return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::RG16F: return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32F: return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::Depth24: return d24;
        case TextureFormat::Depth32F: return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::SBGRA8: return VK_FORMAT_B8G8R8A8_SRGB;
        case TextureFormat::RGB10A2: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R11G11B10F: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case TextureFormat::Depth24Stencil8: return d24s8;
        case TextureFormat::RG32F: return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::R16: return VK_FORMAT_R16_UNORM;
        case TextureFormat::RG16: return VK_FORMAT_R16G16_UNORM;
        case TextureFormat::RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;
        case TextureFormat::R32UI: return VK_FORMAT_R32_UINT;
        case TextureFormat::RGBA8Snorm: return VK_FORMAT_R8G8B8A8_SNORM;
        case TextureFormat::BC1: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::BC1_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case TextureFormat::BC2: return VK_FORMAT_BC2_UNORM_BLOCK;
        case TextureFormat::BC2_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
        case TextureFormat::BC3: return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::BC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
        case TextureFormat::BC4: return VK_FORMAT_BC4_UNORM_BLOCK;
        case TextureFormat::BC4_SNORM: return VK_FORMAT_BC4_SNORM_BLOCK;
        case TextureFormat::BC5: return VK_FORMAT_BC5_UNORM_BLOCK;
        case TextureFormat::BC5_SNORM: return VK_FORMAT_BC5_SNORM_BLOCK;
        case TextureFormat::BC6H_UF16: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case TextureFormat::BC6H_SF16: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case TextureFormat::BC7: return VK_FORMAT_BC7_UNORM_BLOCK;
        case TextureFormat::BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
        case TextureFormat::Undefined: break;
    }
    return VK_FORMAT_UNDEFINED;
}
bool Core::formatUsable(TextureFormat f, TextureUsage u) const {
    if (f == TextureFormat::Undefined || (isCompressed(f) && !bc)) return false;
    const VkFormat vf = vkFormat(f);
    if (vf == VK_FORMAT_UNDEFINED) return false;
    VkFormatProperties fp{};
    vkGetPhysicalDeviceFormatProperties(phys, vf, &fp);
    VkFormatFeatureFlags need = 0;
    if (hasUsage(u, TextureUsage::Sampled)) need |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (hasUsage(u, TextureUsage::RenderTarget)) need |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (hasUsage(u, TextureUsage::DepthStencil)) need |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (hasUsage(u, TextureUsage::Storage)) need |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    return (fp.optimalTilingFeatures & need) == need;
}

void Core::retire(std::function<void()> fn) {
    std::lock_guard<std::mutex> l(mu);
    retired.push_back({submitted, std::move(fn)});
}
void Core::collect(bool all) {
    std::uint64_t done = 0;
    if (!all && device) vkGetSemaphoreCounterValue(device, timeline, &done);
    std::vector<std::function<void()>> run;
    {
        std::lock_guard<std::mutex> l(mu);
        while (!retired.empty() && (all || retired.front().value <= done)) { run.push_back(std::move(retired.front().fn)); retired.pop_front(); }
    }
    for (auto& f : run) f();
}
VkDescriptorPool Core::makePool() {
    const VkDescriptorPoolSize sizes[4] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4096 * kMaxUniformSlots},
                                           {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 * kMaxTextureSlots},
                                           {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 * kMaxStorageImageSlots},
                                           {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 * kMaxStorageBufferSlots}};
    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 4096; ci.poolSizeCount = 4; ci.pPoolSizes = sizes;
    VkDescriptorPool p{};
    VK_CHECK(vkCreateDescriptorPool(device, &ci, nullptr, &p));
    return p;
}
VkDescriptorSet Core::allocSet(VkDescriptorSetLayout layout) {
    auto& v = pools[frame];
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (poolIndex[frame] >= v.size()) v.push_back(makePool());
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = v[poolIndex[frame]]; ai.descriptorSetCount = 1; ai.pSetLayouts = &layout;
        VkDescriptorSet s{};
        const VkResult r = vkAllocateDescriptorSets(device, &ai, &s);
        if (r == VK_SUCCESS) return s;
        if (r == VK_ERROR_OUT_OF_POOL_MEMORY || r == VK_ERROR_FRAGMENTED_POOL) { ++poolIndex[frame]; continue; }
        VK_CHECK(r);
    }
    throw std::runtime_error("Vulkan: cannot allocate descriptor set");
}
void Core::beginFrame() {
    frame = (frame + 1) % kFrames;
    std::uint64_t v = frameValue[frame];
    if (v) {
        VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
        wi.semaphoreCount = 1; wi.pSemaphores = &timeline; wi.pValues = &v;
        VK_CHECK(vkWaitSemaphores(device, &wi, UINT64_MAX));
    }
    for (auto p : pools[frame]) vkResetDescriptorPool(device, p, 0);
    poolIndex[frame] = 0;
    resolveTimings(frame);
    collect();
}

// Reads back the queries this slot recorded the last time it was used (the timeline wait in beginFrame guarantees they are done).
void Core::resolveTimings(std::uint32_t slot) {
    if (timestamps && queryCount[slot] > 0) {
        std::uint64_t t[kMaxTimedScopes * 2];
        const std::uint32_t n = queryCount[slot];
        const VkResult r = vkGetQueryPoolResults(device, queryPool[slot], 0, n, sizeof(std::uint64_t) * n, t, sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
        if (r == VK_SUCCESS) {   // VK_NOT_READY = the list was recorded but never submitted: keep the previous result
            resolved.clear();
            for (std::uint32_t i = 0; i < scopeCount[slot]; ++i) {
                const TimedScope& s = scopes[slot][i];
                GpuScopeTiming g;
                std::memcpy(g.name, s.name, sizeof g.name);
                g.depth = s.depth;
                g.ms = static_cast<float>(static_cast<double>((t[s.q1] - t[s.q0]) & tsMask) * tsNsPerTick * 1e-6);
                resolved.push_back(g);
            }
            ++resolvedSerial;
        }
    }
    scopeCount[slot] = 0; queryCount[slot] = 0; openDepth[slot] = 0;
    queryReset[slot] = false;
}

Core::~Core() {
    if (device) {
        vkDeviceWaitIdle(device);
        collect(true);
        if (fbView) vkDestroyImageView(device, fbView, nullptr);
        if (fbImage) vmaDestroyImage(vma, fbImage, fbAlloc);
        if (fbBuffer) vmaDestroyBuffer(vma, fbBuffer, fbBufAlloc);
        if (defaultSampler) vkDestroySampler(device, defaultSampler, nullptr);
        for (auto p : queryPool) if (p) vkDestroyQueryPool(device, p, nullptr);
        for (auto& v : pools) for (auto p : v) vkDestroyDescriptorPool(device, p, nullptr);
        if (immPool) vkDestroyCommandPool(device, immPool, nullptr);
        if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (uboLayout) vkDestroyDescriptorSetLayout(device, uboLayout, nullptr);
        if (texLayout) vkDestroyDescriptorSetLayout(device, texLayout, nullptr);
        if (storImgLayout) vkDestroyDescriptorSetLayout(device, storImgLayout, nullptr);
        if (storBufLayout) vkDestroyDescriptorSetLayout(device, storBufLayout, nullptr);
        if (timeline) vkDestroySemaphore(device, timeline, nullptr);
        if (vma) vmaDestroyAllocator(vma);
        vkDestroyDevice(device, nullptr);
    }
    if (instance) {
        if (messenger && destroyMessenger) destroyMessenger(instance, messenger, nullptr);
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
}

void Core::init(const DeviceCreateInfo& info, Backend tierIn) {
    tier = tierIn;
    const bool want14 = tier == Backend::Vulkan14;
    const std::uint32_t needApi = want14 ? VK_API_VERSION_1_4 : VK_API_VERSION_1_2;

    // ---- instance
    std::uint32_t instVersion = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&instVersion);
    if (instVersion < needApi)
        throw std::runtime_error(want14 ? "Vulkan 1.4 runtime not found (loader reports an older version). Update the Vulkan SDK/driver or use ANVIL_BACKEND=vulkan12"
                                        : "Vulkan 1.2 runtime not found");
    std::uint32_t n = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> instExt(n);
    vkEnumerateInstanceExtensionProperties(nullptr, &n, instExt.data());
    auto hasInstExt = [&](const char* name) { for (auto& e : instExt) if (!std::strcmp(e.extensionName, name)) return true; return false; };

    std::vector<const char*> extensions;
    if (info.nativeWindow) {
        std::uint32_t gc = 0;
        const char** ge = glfwGetRequiredInstanceExtensions(&gc);
        if (!ge) throw std::runtime_error("GLFW reports no Vulkan support (Vulkan loader missing?)");
        for (std::uint32_t i = 0; i < gc; ++i) extensions.push_back(ge[i]);
    }
    if (hasInstExt("VK_EXT_swapchain_colorspace")) extensions.push_back("VK_EXT_swapchain_colorspace");

    std::vector<const char*> layers;
    bool debugUtils = false;
    if (info.validation) {
        std::uint32_t lc = 0;
        vkEnumerateInstanceLayerProperties(&lc, nullptr);
        std::vector<VkLayerProperties> lp(lc);
        vkEnumerateInstanceLayerProperties(&lc, lp.data());
        for (auto& l : lp) if (!std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation")) { layers.push_back("VK_LAYER_KHRONOS_validation"); break; }
        if (layers.empty()) ANVIL_LOG("Vulkan: validation requested but VK_LAYER_KHRONOS_validation is not installed");
    }
    if (hasInstExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) { extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); debugUtils = true; }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = info.appName.c_str(); app.pEngineName = "ANVIL"; app.engineVersion = 1;
    app.apiVersion = needApi;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()); ici.ppEnabledExtensionNames = extensions.data();
    ici.enabledLayerCount = static_cast<std::uint32_t>(layers.size()); ici.ppEnabledLayerNames = layers.data();
    VK_CHECK(vkCreateInstance(&ici, nullptr, &instance));

    if (debugUtils && !layers.empty()) {
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (create) {
            VkDebugUtilsMessengerCreateInfoEXT mi{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            mi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            mi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            mi.pfnUserCallback = debugCallback;
            create(instance, &mi, nullptr, &messenger);
        }
    }
    caps.validationEnabled = !layers.empty();

    if (info.nativeWindow)
        VK_CHECK(glfwCreateWindowSurface(instance, static_cast<GLFWwindow*>(info.nativeWindow), nullptr, &surface));

    // ---- physical device
    std::uint32_t pc = 0;
    vkEnumeratePhysicalDevices(instance, &pc, nullptr);
    std::vector<VkPhysicalDevice> devs(pc);
    vkEnumeratePhysicalDevices(instance, &pc, devs.data());
    if (devs.empty()) throw std::runtime_error("Vulkan: no physical devices found");

    struct Cand { VkPhysicalDevice pd; std::uint32_t family; long long score; int index; };
    std::vector<Cand> cands;
    std::string rejected;
    for (std::size_t i = 0; i < devs.size(); ++i) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(devs[i], &p);
        auto reject = [&](const char* why) { rejected += std::string("\n  ") + p.deviceName + ": " + why; };
        if (p.apiVersion < needApi) { reject(want14 ? "no Vulkan 1.4 driver" : "no Vulkan 1.2 driver"); continue; }
        const bool software = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
        if (software && !info.allowSoftwareAdapter) { reject("software adapter"); continue; }

        std::uint32_t ec = 0;
        vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &ec, nullptr);
        std::vector<VkExtensionProperties> ext(ec);
        vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &ec, ext.data());
        auto hasExt = [&](const char* name) { for (auto& e : ext) if (!std::strcmp(e.extensionName, name)) return true; return false; };
        if (surface && !hasExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) { reject("no VK_KHR_swapchain"); continue; }
        if (p.apiVersion < VK_API_VERSION_1_3 &&
            !(hasExt(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) && hasExt(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))) { reject("no dynamic rendering / synchronization2"); continue; }

        VkPhysicalDeviceVulkan12Features q12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 q2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        q2.pNext = &q12;
        vkGetPhysicalDeviceFeatures2(devs[i], &q2);
        if (!q12.timelineSemaphore) { reject("no timeline semaphores"); continue; }

        std::uint32_t qc = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qc, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qc);
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qc, qf.data());
        int family = -1;
        for (std::uint32_t q = 0; q < qc; ++q) {
            if (!(qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            if (surface) {
                VkBool32 ok = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], q, surface, &ok);
                if (!ok) continue;
            }
            family = static_cast<int>(q); break;
        }
        if (family < 0) { reject("no graphics+present queue"); continue; }

        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(devs[i], &mp);
        std::uint64_t vram = 0;
        for (std::uint32_t h = 0; h < mp.memoryHeapCount; ++h) if (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) vram += mp.memoryHeaps[h].size;
        long long score = static_cast<long long>(vram >> 20);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000000;
        else if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100000;
        cands.push_back({devs[i], static_cast<std::uint32_t>(family), score, static_cast<int>(i)});
    }
    if (cands.empty()) throw std::runtime_error("Vulkan: no suitable GPU found:" + rejected);
    const Cand* pick = nullptr;
    if (info.preferredAdapter >= 0) {
        for (auto& c : cands) if (c.index == info.preferredAdapter) pick = &c;
        if (!pick) throw std::runtime_error("Vulkan: preferred adapter " + std::to_string(info.preferredAdapter) + " is not suitable:" + rejected);
    } else {
        pick = &cands[0];
        for (auto& c : cands) if (c.score > pick->score) pick = &c;
    }
    phys = pick->pd; queueFamily = pick->family;
    vkGetPhysicalDeviceProperties(phys, &props);
    deviceApi = props.apiVersion;

    // ---- logical device
    std::vector<const char*> devExt;
    if (surface) devExt.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (deviceApi < VK_API_VERSION_1_3) { devExt.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME); devExt.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME); }

    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(phys, &supported);
    anisotropy = supported.samplerAnisotropy == VK_TRUE;
    bc = supported.textureCompressionBC == VK_TRUE;
    cubeArray = supported.imageCubeArray == VK_TRUE;
    storageExtFormats = supported.shaderStorageImageExtendedFormats == VK_TRUE;   // rg16f, r11g11b10f, r8, ... as storage images
    storageNoFormat = supported.shaderStorageImageReadWithoutFormat == VK_TRUE && supported.shaderStorageImageWriteWithoutFormat == VK_TRUE;

    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.dynamicRendering = VK_TRUE; f13.synchronization2 = VK_TRUE;
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dr{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
    dr.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceSynchronization2FeaturesKHR s2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR};
    s2.synchronization2 = VK_TRUE;
    {   // optional Vulkan 1.2 feature: gl_Layer / SV_RenderTargetArrayIndex from the vertex stage (one-pass cubemap / array rendering)
        VkPhysicalDeviceVulkan12Features q12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 q2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        q2.pNext = &q12;
        vkGetPhysicalDeviceFeatures2(phys, &q2);
        shaderOutLayer = q12.shaderOutputLayer == VK_TRUE;
    }
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.timelineSemaphore = VK_TRUE;
    f12.shaderOutputLayer = shaderOutLayer ? VK_TRUE : VK_FALSE;
    if (deviceApi >= VK_API_VERSION_1_3) f12.pNext = &f13;
    else { f12.pNext = &dr; dr.pNext = &s2; }
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f12;
    f2.features.samplerAnisotropy = anisotropy ? VK_TRUE : VK_FALSE;
    f2.features.textureCompressionBC = bc ? VK_TRUE : VK_FALSE;
    f2.features.imageCubeArray = cubeArray ? VK_TRUE : VK_FALSE;
    f2.features.shaderStorageImageExtendedFormats = storageExtFormats ? VK_TRUE : VK_FALSE;
    f2.features.shaderStorageImageReadWithoutFormat = storageNoFormat ? VK_TRUE : VK_FALSE;
    f2.features.shaderStorageImageWriteWithoutFormat = storageNoFormat ? VK_TRUE : VK_FALSE;

    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queueFamily; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &f2;
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<std::uint32_t>(devExt.size()); dci.ppEnabledExtensionNames = devExt.data();
    VK_CHECK(vkCreateDevice(phys, &dci, nullptr, &device));
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    auto load = [&](const char* core, const char* khr) -> PFN_vkVoidFunction {
        PFN_vkVoidFunction f = vkGetDeviceProcAddr(device, core);
        return f ? f : vkGetDeviceProcAddr(device, khr);
    };
    cmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(load("vkCmdBeginRendering", "vkCmdBeginRenderingKHR"));
    cmdEndRendering = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(load("vkCmdEndRendering", "vkCmdEndRenderingKHR"));
    cmdBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(load("vkCmdPipelineBarrier2", "vkCmdPipelineBarrier2KHR"));
    queueSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2KHR>(load("vkQueueSubmit2", "vkQueueSubmit2KHR"));
    if (!cmdBeginRendering || !cmdEndRendering || !cmdBarrier2 || !queueSubmit2) throw std::runtime_error("Vulkan: dynamic rendering / synchronization2 entry points missing");
    cmdWriteTimestamp2 = reinterpret_cast<PFN_vkCmdWriteTimestamp2KHR>(load("vkCmdWriteTimestamp2", "vkCmdWriteTimestamp2KHR"));
    cmdBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    cmdEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));

    // ---- allocator
    VmaAllocatorCreateInfo vci{};
    vci.instance = instance; vci.physicalDevice = phys; vci.device = device;
    vci.vulkanApiVersion = deviceApi >= VK_API_VERSION_1_3 ? VK_API_VERSION_1_3 : VK_API_VERSION_1_2;
    VK_CHECK(vmaCreateAllocator(&vci, &vma));

    // ---- timeline + layouts
    VkSemaphoreTypeCreateInfo tci{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    tci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE; tci.initialValue = 0;
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    sci.pNext = &tci;
    VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &timeline));

    {
        // Compute needs sets 2/3 on top of 0/1: only offered when the queue family, the descriptor limits and the push-constant size allow it.
        {
            const VkPhysicalDeviceLimits& L = props.limits;
            std::uint32_t qn = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, nullptr);
            std::vector<VkQueueFamilyProperties> qp(qn);
            vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qp.data());
            const bool queueOk = queueFamily < qn && (qp[queueFamily].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
            constexpr std::uint32_t kResources = kMaxUniformSlots + kMaxTextureSlots + kMaxStorageImageSlots + kMaxStorageBufferSlots;
            computeOk = queueOk && L.maxBoundDescriptorSets >= 4 && L.maxPushConstantsSize >= kMaxPushConstantBytes &&
                        L.maxPerStageDescriptorUniformBuffers >= kMaxUniformSlots &&
                        L.maxPerStageDescriptorSamplers >= kMaxTextureSlots && L.maxPerStageDescriptorSampledImages >= kMaxTextureSlots &&
                        L.maxPerStageDescriptorStorageImages >= kMaxStorageImageSlots && L.maxPerStageDescriptorStorageBuffers >= kMaxStorageBufferSlots &&
                        L.maxPerStageResources >= kResources;
            if (!computeOk) ANVIL_LOG("Vulkan: compute disabled (queue family / descriptor limits too small for storage sets 2/3)");
        }
        VkDescriptorSetLayoutBinding b[kMaxTextureSlots]{};
        for (unsigned i = 0; i < kMaxUniformSlots; ++i) b[i] = {i, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kSetStages, nullptr};
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        li.bindingCount = kMaxUniformSlots; li.pBindings = b;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &li, nullptr, &uboLayout));
        for (unsigned i = 0; i < kMaxTextureSlots; ++i) b[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, kSetStages, nullptr};
        li.bindingCount = kMaxTextureSlots;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &li, nullptr, &texLayout));
        VkDescriptorSetLayout sets[4] = {uboLayout, texLayout, VK_NULL_HANDLE, VK_NULL_HANDLE};
        if (computeOk) {
            for (unsigned i = 0; i < kMaxStorageImageSlots; ++i) b[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
            li.bindingCount = kMaxStorageImageSlots;
            VK_CHECK(vkCreateDescriptorSetLayout(device, &li, nullptr, &storImgLayout));
            for (unsigned i = 0; i < kMaxStorageBufferSlots; ++i) b[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
            li.bindingCount = kMaxStorageBufferSlots;
            VK_CHECK(vkCreateDescriptorSetLayout(device, &li, nullptr, &storBufLayout));
            sets[2] = storImgLayout; sets[3] = storBufLayout;
        }
        const VkPushConstantRange pcr{kPushStages, 0, kMaxPushConstantBytes};   // one range for every stage: push constants survive pipeline-type switches
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pli.setLayoutCount = computeOk ? 4u : 2u; pli.pSetLayouts = sets; pli.pushConstantRangeCount = 1; pli.pPushConstantRanges = &pcr;
        VK_CHECK(vkCreatePipelineLayout(device, &pli, nullptr, &pipelineLayout));
    }
    {
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pi.queueFamilyIndex = queueFamily;
        VK_CHECK(vkCreateCommandPool(device, &pi, nullptr, &immPool));
    }
    {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = si.minFilter = VK_FILTER_LINEAR; si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.maxLod = VK_LOD_CLAMP_NONE;
        VK_CHECK(vkCreateSampler(device, &si, nullptr, &defaultSampler));
    }

    // ---- depth format fallbacks
    auto supportsDepth = [&](VkFormat f) {
        VkFormatProperties fp{}; vkGetPhysicalDeviceFormatProperties(phys, f, &fp);
        return (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
    };
    d24 = supportsDepth(VK_FORMAT_X8_D24_UNORM_PACK32) ? VK_FORMAT_X8_D24_UNORM_PACK32 : VK_FORMAT_D32_SFLOAT;
    d24s8 = supportsDepth(VK_FORMAT_D24_UNORM_S8_UINT) ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;

    // ---- fallback resources for unbound descriptor slots
    {
        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D; ii.format = VK_FORMAT_R8G8B8A8_UNORM; ii.extent = {1, 1, 1}; ii.mipLevels = 1; ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT; ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo ai{}; ai.usage = VMA_MEMORY_USAGE_AUTO;
        VK_CHECK(vmaCreateImage(vma, &ii, &ai, &fbImage, &fbAlloc, nullptr));
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = fbImage; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = ii.format; vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(device, &vi, nullptr, &fbView));
        const std::uint8_t white[4] = {255, 255, 255, 255};
        Staging st = makeStaging(white, 4);
        immediate([&](VkCommandBuffer cmd) {
            VkImageMemoryBarrier2 b = makeBarrier(fbImage, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                scopeFor(VK_IMAGE_LAYOUT_UNDEFINED), scopeFor(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
            barriers(cmd, &b, 1);
            VkBufferImageCopy rg{}; rg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; rg.imageExtent = {1, 1, 1};
            vkCmdCopyBufferToImage(cmd, st.buf, fbImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rg);
            b = makeBarrier(fbImage, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                scopeFor(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), scopeFor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
            barriers(cmd, &b, 1);
        });
        destroyStaging(st);

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = kFallbackUboSize; bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo bai{}; bai.usage = VMA_MEMORY_USAGE_AUTO;
        VK_CHECK(vmaCreateBuffer(vma, &bi, &bai, &fbBuffer, &fbBufAlloc, nullptr));
        immediate([&](VkCommandBuffer cmd) { vkCmdFillBuffer(cmd, fbBuffer, 0, VK_WHOLE_SIZE, 0); });
    }

    // ---- capabilities
    caps.backend = tier;
    caps.deviceName = props.deviceName;
    caps.apiVersion = std::to_string(VK_API_VERSION_MAJOR(deviceApi)) + "." + std::to_string(VK_API_VERSION_MINOR(deviceApi)) + "." + std::to_string(VK_API_VERSION_PATCH(deviceApi));
    caps.maxTextureSize = static_cast<int>(props.limits.maxImageDimension2D);
    {
        const VkSampleCountFlags sc = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
        int s = 1;
        for (int c = 2; c <= 64; c *= 2) if (sc & static_cast<VkSampleCountFlags>(c)) s = c;
        caps.maxSamples = s;
    }
    caps.debugOutput = messenger != VK_NULL_HANDLE;
    caps.directStateAccess = false;
    caps.framesInFlight = kFrames;
    caps.discreteGpu = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    caps.maxAnisotropy = anisotropy ? props.limits.maxSamplerAnisotropy : 1.0f;
    caps.textureCompressionBC = bc; caps.cubeArrays = cubeArray; caps.shaderOutputLayer = shaderOutLayer;
    caps.maxArrayLayers = static_cast<int>(props.limits.maxImageArrayLayers);
    caps.maxCubeSize = static_cast<int>(props.limits.maxImageDimensionCube);
    caps.maxTexture3DSize = static_cast<int>(props.limits.maxImageDimension3D);
    for (int i = 0; i < 3; ++i) caps.maxComputeWorkGroupSize[i] = props.limits.maxComputeWorkGroupSize[i];
    caps.maxComputeInvocations = props.limits.maxComputeWorkGroupInvocations;
    caps.compute = computeOk; caps.storageImages = computeOk;   // M43 step 4: compute pipelines, dispatch, storage images / buffers
    {   // GPU timestamps: needs valid bits on the graphics queue, a non-zero tick period and the synchronization2 write entry point
        std::uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qprops.data());
        const std::uint32_t bits = queueFamily < qn ? qprops[queueFamily].timestampValidBits : 0;
        if (bits >= 32 && props.limits.timestampPeriod > 0.0f && cmdWriteTimestamp2) {
            tsMask = bits >= 64 ? ~0ull : ((1ull << bits) - 1ull);
            tsNsPerTick = static_cast<double>(props.limits.timestampPeriod);
            bool ok = true;
            for (std::uint32_t i = 0; i < kFrames && ok; ++i) {
                VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
                qi.queryType = VK_QUERY_TYPE_TIMESTAMP; qi.queryCount = kMaxTimedScopes * 2;
                ok = vkCreateQueryPool(device, &qi, nullptr, &queryPool[i]) == VK_SUCCESS;
            }
            timestamps = ok;
            if (!ok) for (auto& p : queryPool) { if (p) vkDestroyQueryPool(device, p, nullptr); p = VK_NULL_HANDLE; }
            else resolved.reserve(kMaxTimedScopes);
        }
    }
    {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mp);
        for (std::uint32_t h = 0; h < mp.memoryHeapCount; ++h) if (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) caps.dedicatedVideoMemory += mp.memoryHeaps[h].size;
    }
    ANVIL_LOG("Vulkan %s | %s%s", caps.apiVersion.c_str(), props.deviceName, caps.validationEnabled ? " | validation ON" : "");
}

// ---------------------------------------------------------------------------------------------------------------------
class Buffer final : public rhi::Buffer {
public:
    Buffer(std::shared_ptr<Core> core, const BufferDesc& d, const void* init) : core_(std::move(core)), size_(d.size) {
        if (size_ == 0) throw std::invalid_argument("Vulkan: zero-sized buffer");
        VkBufferUsageFlags u = 0;
        switch (d.usage) {
            case BufferUsage::Vertex: u = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
            case BufferUsage::Index: u = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
            case BufferUsage::Uniform: u = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
            case BufferUsage::Storage: u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
            case BufferUsage::Readback: u = VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
            case BufferUsage::Indirect: u = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;   // v3: dispatchIndirect args, GPU-written
        }
        usage_ = d.usage;
        readback_ = d.usage == BufferUsage::Readback;
        ring_ = d.dynamic && !readback_;
        host_ = d.dynamic || readback_;
        stride_ = ring_ ? alignUp(size_, 256) : size_;
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = ring_ ? stride_ * kFrames : size_;
        bi.usage = u | VK_BUFFER_USAGE_TRANSFER_DST_BIT; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_AUTO;
        if (host_) ai.flags = (readback_ ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo info{};
        VK_CHECK(vmaCreateBuffer(core_->vma, &bi, &ai, &buf_, &alloc_, &info));
        mapped_ = static_cast<std::uint8_t*>(info.pMappedData);
        if (ring_) {
            shadow_.assign(size_, 0);
            if (init) std::memcpy(shadow_.data(), init, size_);
            version_ = 1; slotVersion_.fill(0);
            sync();
        } else if (host_) {
            if (init) { std::memcpy(mapped_, init, size_); vmaFlushAllocation(core_->vma, alloc_, 0, VK_WHOLE_SIZE); }
        } else if (init) {
            core_->uploadBuffer(buf_, 0, init, size_);
        }
    }
    ~Buffer() override {
        Core* c = core_.get(); VkBuffer b = buf_; VmaAllocation a = alloc_;
        c->retire([c, b, a] { vmaDestroyBuffer(c->vma, b, a); });
    }
    std::size_t size() const noexcept override { return size_; }
    void update(std::size_t offset, const void* data, std::size_t bytes) override {
        if (offset + bytes > size_) throw std::out_of_range("Vulkan: Buffer::update out of range");
        if (bytes == 0) return;
        if (ring_) { std::memcpy(shadow_.data() + offset, data, bytes); ++version_; sync(); }
        else if (host_) { std::memcpy(mapped_ + offset, data, bytes); vmaFlushAllocation(core_->vma, alloc_, offset, bytes); }
        else core_->uploadBuffer(buf_, offset, data, bytes);
    }
    // Brings this frame's slot of a `dynamic` buffer up to date (cheap no-op otherwise).
    void sync() {
        if (!ring_) return;
        const std::uint32_t f = core_->frame;
        if (slotVersion_[f] == version_) return;
        std::memcpy(mapped_ + f * stride_, shadow_.data(), size_);
        vmaFlushAllocation(core_->vma, alloc_, f * stride_, size_);
        slotVersion_[f] = version_;
    }
    VkBuffer handle() const noexcept { return buf_; }
    VkDeviceSize slotOffset() const noexcept { return ring_ ? core_->frame * stride_ : 0; }
    BufferUsage usage() const noexcept { return usage_; }
    bool gpuWritable() const noexcept { return !ring_; }   // `dynamic` rings are CPU-updated per frame: shaders only read them

private:
    std::shared_ptr<Core> core_;
    VkBuffer buf_ = VK_NULL_HANDLE; VmaAllocation alloc_ = nullptr;
    std::size_t size_ = 0; VkDeviceSize stride_ = 0;
    BufferUsage usage_ = BufferUsage::Vertex;
    bool ring_ = false, host_ = false, readback_ = false;
    std::uint8_t* mapped_ = nullptr;
    std::vector<std::uint8_t> shadow_;
    std::uint64_t version_ = 0; std::array<std::uint64_t, kFrames> slotVersion_{};
};

class Texture final : public rhi::Texture {
public:
    // Owned image created through VMA. Legacy path: optional tightly packed mip-0 data of a 2D texture.
    Texture(std::shared_ptr<Core> core, const TextureDesc& d, const void* init) : core_(std::move(core)) {
        TextureSubresourceData s; s.data = init;
        create(d, init ? &s : nullptr, init ? 1 : 0);
    }
    // v3: 2D / cube / array / cube array / 3D, per-subresource data (see TextureSubresourceData).
    Texture(std::shared_ptr<Core> core, const TextureDesc& d, const TextureSubresourceData* data, std::size_t count) : core_(std::move(core)) {
        create(d, data, count);
    }
    // Swapchain backbuffer wrapper (image owned by the swapchain).
    Texture(std::shared_ptr<Core> core, VkImage image, VkFormat vkFormat, TextureFormat format, int w, int h, Swapchain* owner)
        : core_(std::move(core)), image_(image), vkFmt_(vkFormat), fmt_(format), w_(w), h_(h), owner_(owner) {
        createViews();
    }
    ~Texture() override {
        Core* c = core_.get();
        std::vector<VkImageView> views;
        if (view_) views.push_back(view_);
        if (sampleView_ && sampleView_ != view_) views.push_back(sampleView_);
        for (auto& kv : views_) views.push_back(kv.second);
        VkImage img = owner_ ? VK_NULL_HANDLE : image_; VmaAllocation a = alloc_;
        c->retire([c, views, img, a] {
            for (auto v : views) vkDestroyImageView(c->device, v, nullptr);
            if (img) vmaDestroyImage(c->vma, img, a);
        });
    }
    int width() const noexcept override { return w_; }
    int height() const noexcept override { return h_; }
    TextureFormat format() const noexcept override { return fmt_; }
    std::uintptr_t nativeHandle() const noexcept override { return reinterpret_cast<std::uintptr_t>(sampleView_); }
    TextureDimension dimension() const noexcept override { return dim_; }
    int levels() const noexcept override { return levels_; }
    int layers() const noexcept override { return layers_; }
    int depth() const noexcept override { return d_; }

    // Cached image views over any (type, aspect, mip range, layer range): render-target views, sub-range sampled views.
    struct ViewKey {
        VkImageViewType type; VkImageAspectFlags aspect; std::uint32_t m0, mc, l0, lc;
        bool operator<(const ViewKey& o) const { return std::tie(type, aspect, m0, mc, l0, lc) < std::tie(o.type, o.aspect, o.m0, o.mc, o.l0, o.lc); }
    };
    VkImageView getView(VkImageViewType type, VkImageAspectFlags aspect, std::uint32_t m0, std::uint32_t mc, std::uint32_t l0, std::uint32_t lc) {
        const ViewKey k{type, aspect, m0, mc, l0, lc};
        auto it = views_.find(k);
        if (it != views_.end()) return it->second;
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = image_; vi.viewType = type; vi.format = vkFmt_;
        vi.subresourceRange = {aspect, m0, mc, l0, lc};
        VkImageView v{};
        VK_CHECK(vkCreateImageView(core_->device, &vi, nullptr, &v));
        views_[k] = v;
        return v;
    }

    // ---- per-subresource layout tracking. sub_ is empty while every mip/layer shares layout_ (the common case: one barrier for the lot). ----
    VkImageLayout layoutAt(std::uint32_t mip, std::uint32_t layer) const { return sub_.empty() ? layout_ : sub_[static_cast<std::size_t>(mip) * layers_ + layer]; }
    bool allIn(VkImageLayout want, std::uint32_t m0, std::uint32_t mc, std::uint32_t l0, std::uint32_t lc) const {
        if (sub_.empty()) return layout_ == want;
        for (std::uint32_t m = m0; m < m0 + mc; ++m) for (std::uint32_t l = l0; l < l0 + lc; ++l) if (layoutAt(m, l) != want) return false;
        return true;
    }
    void setLayout(std::uint32_t m0, std::uint32_t mc, std::uint32_t l0, std::uint32_t lc, VkImageLayout want) {
        if (m0 == 0 && mc >= static_cast<std::uint32_t>(levels_) && l0 == 0 && lc >= static_cast<std::uint32_t>(layers_)) { sub_.clear(); layout_ = want; return; }
        if (sub_.empty()) sub_.assign(static_cast<std::size_t>(levels_) * layers_, layout_);
        for (std::uint32_t m = m0; m < m0 + mc; ++m) for (std::uint32_t l = l0; l < l0 + lc; ++l) sub_[static_cast<std::size_t>(m) * layers_ + l] = want;
        bool same = true;
        for (auto v : sub_) if (v != sub_[0]) { same = false; break; }
        if (same) { layout_ = sub_[0]; sub_.clear(); }
    }
    static bool readOnlyLayout(VkImageLayout l) {
        return l == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || l == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL || l == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    // Appends the barriers that move a subresource range to `want` and records the new layout. mc/lc = 0 -> "all remaining".
    // discard: contents are not needed (old layout UNDEFINED); the source scope still comes from the real previous layout.
    void collect(std::vector<VkImageMemoryBarrier2>& out, VkImageAspectFlags aspect, std::uint32_t m0, std::uint32_t mc,
                 std::uint32_t l0, std::uint32_t lc, VkImageLayout want, bool discard) {
        const std::uint32_t L = static_cast<std::uint32_t>(levels_), Y = static_cast<std::uint32_t>(layers_);
        if (m0 >= L || l0 >= Y) return;
        if (mc == 0 || m0 + mc > L) mc = L - m0;
        if (lc == 0 || l0 + lc > Y) lc = Y - l0;
        auto emit = [&](std::uint32_t m, std::uint32_t mn, std::uint32_t l, std::uint32_t ln, VkImageLayout old) {
            if (old == want && !discard && (readOnlyLayout(want) || want == VK_IMAGE_LAYOUT_GENERAL)) return;   // nothing to wait for (GENERAL: storage hazards are tracked by the command list)
            out.push_back(makeBarrier(image_, aspect, m, mn, discard ? VK_IMAGE_LAYOUT_UNDEFINED : old, want, scopeFor(old), scopeFor(want), l, ln));
        };
        if (sub_.empty()) emit(m0, mc, l0, lc, layout_);
        else {
            for (std::uint32_t m = m0; m < m0 + mc; ++m) {
                std::uint32_t l = l0;
                while (l < l0 + lc) {
                    const VkImageLayout old = layoutAt(m, l);
                    std::uint32_t r = l + 1;
                    while (r < l0 + lc && layoutAt(m, r) == old) ++r;
                    emit(m, 1, l, r - l, old);
                    l = r;
                }
            }
        }
        setLayout(m0, mc, l0, lc, want);
    }
    void transition(VkCommandBuffer cmd, VkImageAspectFlags aspect, std::uint32_t m0, std::uint32_t mc, std::uint32_t l0, std::uint32_t lc, VkImageLayout want, bool discard) {
        std::vector<VkImageMemoryBarrier2> bars;
        collect(bars, aspect, m0, mc, l0, lc, want, discard);
        if (!bars.empty()) core_->barriers(cmd, bars.data(), static_cast<std::uint32_t>(bars.size()));
    }

    std::shared_ptr<Core> core_;
    VkImage image_ = VK_NULL_HANDLE; VmaAllocation alloc_ = nullptr;
    // view_ = 2D view of layer 0 (attachment use, and sampling of plain 2D textures); sampleView_ = full view of the texture's own type
    // (cube / 2D array / cube array / 3D) over every mip and layer.
    VkImageView view_ = VK_NULL_HANDLE, sampleView_ = VK_NULL_HANDLE;
    VkFormat vkFmt_ = VK_FORMAT_UNDEFINED; TextureFormat fmt_ = TextureFormat::RGBA8;
    int w_ = 1, h_ = 1, d_ = 1, levels_ = 1, layers_ = 1;
    TextureDimension dim_ = TextureDimension::Tex2D;
    TextureUsage usage_ = TextureUsage::Sampled;   // as requested at creation (bindStorageTexture checks Storage)
    // Layout of every subresource while sub_ is empty; sub_ (mip * layers + layer) only exists while the subresources differ.
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    std::vector<VkImageLayout> sub_;
    Swapchain* owner_ = nullptr;
    std::map<ViewKey, VkImageView> views_;

private:
    void create(const TextureDesc& d, const TextureSubresourceData* data, std::size_t count) {
        if (d.width <= 0 || d.height <= 0) throw std::invalid_argument("Vulkan: invalid texture size");
        fmt_ = d.format; w_ = d.width; h_ = d.height; dim_ = d.dimension;
        vkFmt_ = core_->vkFormat(fmt_);
        if (vkFmt_ == VK_FORMAT_UNDEFINED) throw std::invalid_argument("Vulkan: undefined texture format");
        const bool depth = isDepthFormat(fmt_), comp = isCompressed(fmt_);
        if (comp && !core_->bc) throw std::invalid_argument("Vulkan: block-compressed (BCn) formats are not supported by this device");
        const auto& lim = core_->props.limits;
        d_ = 1; layers_ = 1;
        switch (dim_) {
            case TextureDimension::Tex2D: break;
            case TextureDimension::Cube:
                if (w_ != h_) throw std::invalid_argument("Vulkan: cube textures must be square");
                if (w_ > static_cast<int>(lim.maxImageDimensionCube)) throw std::invalid_argument("Vulkan: cube texture too large");
                layers_ = 6; break;
            case TextureDimension::Tex2DArray: layers_ = std::max(1, d.layers); break;
            case TextureDimension::CubeArray:
                if (!core_->cubeArray) throw std::invalid_argument("Vulkan: cube map arrays are not supported by this device");
                if (w_ != h_) throw std::invalid_argument("Vulkan: cube textures must be square");
                if (w_ > static_cast<int>(lim.maxImageDimensionCube)) throw std::invalid_argument("Vulkan: cube texture too large");
                layers_ = 6 * std::max(1, d.layers); break;
            case TextureDimension::Tex3D:
                d_ = std::max(1, d.depth);
                if (w_ > static_cast<int>(lim.maxImageDimension3D) || h_ > static_cast<int>(lim.maxImageDimension3D) || d_ > static_cast<int>(lim.maxImageDimension3D))
                    throw std::invalid_argument("Vulkan: 3D texture too large");
                break;
        }
        if (layers_ > static_cast<int>(lim.maxImageArrayLayers)) throw std::invalid_argument("Vulkan: too many array layers");
        const bool target = hasUsage(d.usage, TextureUsage::RenderTarget) || hasUsage(d.usage, TextureUsage::DepthStencil);
        if (d.samples > 1 && dim_ != TextureDimension::Tex2D) throw std::invalid_argument("Vulkan: multisampling needs a plain 2D texture");
        if (comp && target) throw std::invalid_argument("Vulkan: block-compressed formats cannot be render targets");
        if (dim_ == TextureDimension::Tex3D && target) throw std::invalid_argument("Vulkan: 3D render targets are not supported (use Storage)");
        usage_ = d.usage;
        if (hasUsage(d.usage, TextureUsage::Storage)) {
            if (depth || comp) throw std::invalid_argument("Vulkan: depth and block-compressed formats cannot be storage images");
            if (d.samples > 1) throw std::invalid_argument("Vulkan: multisampled textures cannot be storage images");
            if (!core_->formatUsable(fmt_, TextureUsage::Storage))
                throw std::invalid_argument("Vulkan: this format cannot be used as a storage image on this device (see Device::supportsFormat)");
        }
        int maxLevels = 1;
        for (int m = std::max({w_, h_, d_}); m > 1; m >>= 1) ++maxLevels;
        levels_ = std::clamp(d.levels, 1, maxLevels);

        // ---- pack the initial data first: nothing is allocated if the data is invalid
        std::vector<VkBufferImageCopy> regions; std::vector<std::uint8_t> blob; bool onlyMip0 = true;
        if (data && count) {
            if (depth) throw std::invalid_argument("Vulkan: initial data for depth textures is not supported");
            const std::size_t srcBpp = comp ? 0 : bytesPerPixel(fmt_);
            const std::size_t dstBpp = fmt_ == TextureFormat::RGB8 ? 4 : srcBpp;
            for (std::size_t k = 0; k < count; ++k) {
                const TextureSubresourceData& e = data[k];
                if (!e.data) continue;
                if (e.mip >= static_cast<std::uint32_t>(levels_) || e.layer >= static_cast<std::uint32_t>(layers_))
                    throw std::out_of_range("Vulkan: texture data addresses a missing mip level or layer");
                const int mw = std::max(1, w_ >> e.mip), mh = std::max(1, h_ >> e.mip), md = std::max(1, d_ >> e.mip);
                const std::size_t rowsPerSlice = comp ? static_cast<std::size_t>((mh + 3) / 4) : static_cast<std::size_t>(mh);
                const std::size_t rowTight = comp ? static_cast<std::size_t>((mw + 3) / 4) * blockBytes(fmt_) : static_cast<std::size_t>(mw) * srcBpp;
                const std::size_t rowDst = comp ? rowTight : static_cast<std::size_t>(mw) * dstBpp;
                const std::size_t rowPitch = e.rowPitch ? e.rowPitch : rowTight;
                if (rowPitch < rowTight) throw std::invalid_argument("Vulkan: texture data rowPitch is smaller than one row");
                const std::size_t rows = rowsPerSlice * static_cast<std::size_t>(md);
                if (e.bytes && e.bytes < rowPitch * (rows - 1) + rowTight) throw std::invalid_argument("Vulkan: texture data is smaller than its subresource");
                const std::size_t off = static_cast<std::size_t>(alignUp(blob.size(), 16));
                blob.resize(off + rowDst * rows);
                const auto* src = static_cast<const std::uint8_t*>(e.data);
                std::uint8_t* dst = blob.data() + off;
                for (std::size_t r = 0; r < rows; ++r) {
                    const std::uint8_t* sr = src + r * rowPitch; std::uint8_t* dr = dst + r * rowDst;
                    if (fmt_ == TextureFormat::RGB8) {   // expanded to RGBA8
                        for (int x = 0; x < mw; ++x) { dr[x * 4 + 0] = sr[x * 3 + 0]; dr[x * 4 + 1] = sr[x * 3 + 1]; dr[x * 4 + 2] = sr[x * 3 + 2]; dr[x * 4 + 3] = 255; }
                    } else std::memcpy(dr, sr, rowTight);
                }
                VkBufferImageCopy rg{};
                rg.bufferOffset = off;
                rg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, e.mip, e.layer, 1};
                rg.imageExtent = {static_cast<std::uint32_t>(mw), static_cast<std::uint32_t>(mh), static_cast<std::uint32_t>(md)};
                regions.push_back(rg);
                if (e.mip != 0) onlyMip0 = false;
            }
        }

        VkImageUsageFlags u = 0;
        if (hasUsage(d.usage, TextureUsage::Sampled)) u |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (target) u |= depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (hasUsage(d.usage, TextureUsage::Storage)) u |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (hasUsage(d.usage, TextureUsage::TransferSrc)) u |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (hasUsage(d.usage, TextureUsage::TransferDst) || !regions.empty()) u |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (levels_ > 1 && !comp) u |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (u == 0) u = VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = dim_ == TextureDimension::Tex3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D; ii.format = vkFmt_;
        if (dim_ == TextureDimension::Cube || dim_ == TextureDimension::CubeArray) ii.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        ii.extent = {static_cast<std::uint32_t>(w_), static_cast<std::uint32_t>(h_), static_cast<std::uint32_t>(d_)};
        ii.mipLevels = static_cast<std::uint32_t>(levels_); ii.arrayLayers = static_cast<std::uint32_t>(layers_);
        ii.samples = static_cast<VkSampleCountFlagBits>(std::max(1, d.samples));
        ii.tiling = VK_IMAGE_TILING_OPTIMAL; ii.usage = u; ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_AUTO;
        if (u & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) ai.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        VK_CHECK(vmaCreateImage(core_->vma, &ii, &ai, &image_, &alloc_, nullptr));
        createViews();

        if (!regions.empty()) {
            // Only mip 0 supplied -> blit-generate the rest (all layers / volume depth). Supplied mips (or BCn) are used as given.
            const bool gen = levels_ > 1 && !comp && onlyMip0;
            Core::Staging st = core_->makeStaging(blob.data(), blob.size());
            core_->immediate([&](VkCommandBuffer cmd) {
                VkImageMemoryBarrier2 b = makeBarrier(image_, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, scopeFor(VK_IMAGE_LAYOUT_UNDEFINED), scopeFor(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
                core_->barriers(cmd, &b, 1);
                vkCmdCopyBufferToImage(cmd, st.buf, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
                if (gen) core_->recordMips(cmd, image_, w_, h_, levels_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layers_, d_);
                else {
                    b = makeBarrier(image_, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        scopeFor(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), scopeFor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                    core_->barriers(cmd, &b, 1);
                }
            });
            core_->destroyStaging(st);
            layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }
    void createViews() {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = image_; vi.format = vkFmt_;
        vi.viewType = dim_ == TextureDimension::Tex3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
        vi.subresourceRange = {attachAspect(fmt_), 0, VK_REMAINING_MIP_LEVELS, 0, 1};
        VK_CHECK(vkCreateImageView(core_->device, &vi, nullptr, &view_));
        if (dim_ == TextureDimension::Tex2D && sampleAspect(fmt_) == attachAspect(fmt_)) { sampleView_ = view_; return; }
        switch (dim_) {
            case TextureDimension::Tex2D: vi.viewType = VK_IMAGE_VIEW_TYPE_2D; break;
            case TextureDimension::Cube: vi.viewType = VK_IMAGE_VIEW_TYPE_CUBE; break;
            case TextureDimension::Tex2DArray: vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
            case TextureDimension::CubeArray: vi.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; break;
            case TextureDimension::Tex3D: vi.viewType = VK_IMAGE_VIEW_TYPE_3D; break;
        }
        vi.subresourceRange = {sampleAspect(fmt_), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        VK_CHECK(vkCreateImageView(core_->device, &vi, nullptr, &sampleView_));
    }
};

class Sampler final : public rhi::Sampler {
public:
    Sampler(std::shared_ptr<Core> core, const SamplerDesc& d) : core_(std::move(core)) {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        const bool mip = d.minFilter == TextureFilter::LinearMipmapLinear;
        si.minFilter = d.minFilter == TextureFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        si.magFilter = d.magFilter == TextureFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        si.mipmapMode = mip ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = toVk(d.wrapU); si.addressModeV = toVk(d.wrapV); si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.anisotropyEnable = (core_->anisotropy && d.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
        si.maxAnisotropy = std::min(d.maxAnisotropy, core_->props.limits.maxSamplerAnisotropy);
        si.compareEnable = d.compare ? VK_TRUE : VK_FALSE; si.compareOp = toVk(d.compareOp);
        si.minLod = 0.0f; si.maxLod = mip ? VK_LOD_CLAMP_NONE : 0.0f;
        si.borderColor = d.whiteBorder ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE : VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        VK_CHECK(vkCreateSampler(core_->device, &si, nullptr, &sampler_));
    }
    ~Sampler() override { Core* c = core_.get(); VkSampler s = sampler_; c->retire([c, s] { vkDestroySampler(c->device, s, nullptr); }); }
    VkSampler handle() const noexcept { return sampler_; }
private:
    std::shared_ptr<Core> core_;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

class Shader final : public rhi::Shader {
public:
    Shader(std::shared_ptr<Core> core, const ShaderDesc& d) : core_(std::move(core)) {
        const bool gfx = !d.vertexSpirv.empty() || !d.fragmentSpirv.empty();
        const bool cmp = !d.computeSpirv.empty();
        if (gfx && cmp) throw std::invalid_argument("Vulkan: a Shader holds either vertex+fragment or a compute stage, not both");
        if (cmp) {   // v3 compute shader: reflect the entry point + local size now, create the module
            comp_info_ = reflectSpirvCompute(d.computeSpirv);
            if (!comp_info_.valid) throw std::invalid_argument("Vulkan: computeSpirv is not a valid SPIR-V module with a GLCompute entry point");
            comp_ = make(d.computeSpirv);
            return;
        }
        if (d.vertexSpirv.empty() || d.fragmentSpirv.empty())
            throw std::runtime_error("Vulkan: ShaderDesc needs vertexSpirv + fragmentSpirv, or computeSpirv (load them with rhi::loadShaderDesc / loadComputeShaderDesc)");
        vert_ = make(d.vertexSpirv); frag_ = make(d.fragmentSpirv);
    }
    ~Shader() override {
        Core* c = core_.get(); VkShaderModule v = vert_, f = frag_, k = comp_;
        c->retire([c, v, f, k] {
            if (v) vkDestroyShaderModule(c->device, v, nullptr);
            if (f) vkDestroyShaderModule(c->device, f, nullptr);
            if (k) vkDestroyShaderModule(c->device, k, nullptr);
        });
    }
    VkShaderModule vert() const noexcept { return vert_; }
    VkShaderModule frag() const noexcept { return frag_; }
    VkShaderModule comp() const noexcept { return comp_; }
    bool isCompute() const noexcept { return comp_ != VK_NULL_HANDLE; }
    const SpirvComputeInfo& computeInfo() const noexcept { return comp_info_; }
private:
    VkShaderModule make(const std::vector<std::uint32_t>& code) {
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = code.size() * sizeof(std::uint32_t); ci.pCode = code.data();
        VkShaderModule m{};
        VK_CHECK(vkCreateShaderModule(core_->device, &ci, nullptr, &m));
        return m;
    }
    std::shared_ptr<Core> core_;
    VkShaderModule vert_ = VK_NULL_HANDLE, frag_ = VK_NULL_HANDLE, comp_ = VK_NULL_HANDLE;
    SpirvComputeInfo comp_info_;
};

class Pipeline final : public rhi::Pipeline {
public:
    Pipeline(std::shared_ptr<Core> core, const std::shared_ptr<rhi::Shader>& shader, const PipelineDesc& d)
        : core_(std::move(core)), shader_(std::static_pointer_cast<Shader>(shader)), desc_(d) {
        if (!shader_) throw std::invalid_argument("Vulkan: createPipeline without shader");
        if (shader_->isCompute()) throw std::invalid_argument("Vulkan: createPipeline got a compute shader (use createComputePipeline)");
        if (d.pushConstantBytes > kMaxPushConstantBytes) throw std::invalid_argument("Vulkan: pushConstantBytes exceeds kMaxPushConstantBytes");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shader_->vert(), "main", nullptr};
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shader_->frag(), "main", nullptr};

        std::vector<VkVertexInputBindingDescription> binds;
        for (auto& b : d.vertexLayout.bindings) binds.push_back({b.binding, b.stride, b.perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX});
        std::vector<VkVertexInputAttributeDescription> attrs;
        for (auto& a : d.vertexLayout.attributes) attrs.push_back({a.location, a.binding, toVk(a.format), a.offset});
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = static_cast<std::uint32_t>(binds.size()); vi.pVertexBindingDescriptions = binds.data();
        vi.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attrs.size()); vi.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = toVk(d.topology);

        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = 1; vp.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.0f;
        const CullMode cm = d.cull ? d.cullMode : CullMode::None;
        rs.cullMode = cm == CullMode::Back ? VK_CULL_MODE_BACK_BIT : cm == CullMode::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
        rs.frontFace = d.frontFaceCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        if (d.depthBiasConstant != 0.0f || d.depthBiasSlope != 0.0f) {
            rs.depthBiasEnable = VK_TRUE; rs.depthBiasConstantFactor = d.depthBiasConstant; rs.depthBiasSlopeFactor = d.depthBiasSlope;
        }

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = static_cast<VkSampleCountFlagBits>(std::max(1, d.samples));

        const bool hasDepth = d.depthFormat != TextureFormat::Undefined;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = (hasDepth && d.depthTest) ? VK_TRUE : VK_FALSE;
        ds.depthWriteEnable = (hasDepth && d.depthTest && d.depthWrite) ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp = toVk(d.depthCompare);

        VkPipelineColorBlendAttachmentState att{};
        att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (d.blend) {
            att.blendEnable = VK_TRUE;
            att.colorBlendOp = att.alphaBlendOp = VK_BLEND_OP_ADD;
            switch (d.blendMode) {
                case BlendMode::Alpha:
                    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
                case BlendMode::Additive:
                    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE; break;
                case BlendMode::Premultiplied:
                    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
            }
        }
        std::vector<VkPipelineColorBlendAttachmentState> atts(d.colorFormats.size(), att);
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = static_cast<std::uint32_t>(atts.size()); cb.pAttachments = atts.data();

        const VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dy.dynamicStateCount = 2; dy.pDynamicStates = dyn;

        std::vector<VkFormat> colorFmts;
        for (auto f : d.colorFormats) colorFmts.push_back(core_->vkFormat(f));
        VkPipelineRenderingCreateInfo ri{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        ri.colorAttachmentCount = static_cast<std::uint32_t>(colorFmts.size()); ri.pColorAttachmentFormats = colorFmts.data();
        if (hasDepth) {
            ri.depthAttachmentFormat = core_->vkFormat(d.depthFormat);
            if (hasStencil(d.depthFormat)) ri.stencilAttachmentFormat = ri.depthAttachmentFormat;
        }

        VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pi.pNext = &ri;
        pi.stageCount = 2; pi.pStages = stages;
        pi.pVertexInputState = &vi; pi.pInputAssemblyState = &ia; pi.pViewportState = &vp; pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms; pi.pDepthStencilState = &ds; pi.pColorBlendState = &cb; pi.pDynamicState = &dy;
        pi.layout = core_->pipelineLayout; pi.renderPass = VK_NULL_HANDLE;
        VK_CHECK(vkCreateGraphicsPipelines(core_->device, VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline_));
    }
    ~Pipeline() override { Core* c = core_.get(); VkPipeline p = pipeline_; c->retire([c, p] { vkDestroyPipeline(c->device, p, nullptr); }); }
    VkPipeline handle() const noexcept { return pipeline_; }
    const PipelineDesc& desc() const noexcept { return desc_; }
private:
    std::shared_ptr<Core> core_;
    std::shared_ptr<Shader> shader_;
    PipelineDesc desc_;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

class ComputePipeline final : public rhi::ComputePipeline {
public:
    ComputePipeline(std::shared_ptr<Core> core, const std::shared_ptr<rhi::Shader>& shader, const ComputePipelineDesc& d)
        : core_(std::move(core)), shader_(std::static_pointer_cast<Shader>(shader)) {
        if (!core_->caps.compute) throw UnsupportedByBackend("Vulkan: compute is not available on this device (queue family or descriptor limits)");
        if (!shader_ || !shader_->isCompute()) throw std::invalid_argument("Vulkan: createComputePipeline needs a shader created from ShaderDesc::computeSpirv");
        if (d.pushConstantBytes > kMaxPushConstantBytes) throw std::invalid_argument("Vulkan: pushConstantBytes exceeds kMaxPushConstantBytes");

        const SpirvComputeInfo& info = shader_->computeInfo();
        if (info.localSizeKnown) for (int i = 0; i < 3; ++i) local_[i] = info.localSize[i];
        else ANVIL_LOG("Vulkan: compute shader '%s' has no literal local_size (LocalSizeId / spec constants): localSize() reports 1x1x1", d.label.c_str());
        sbUsed_ = info.storageBufferUsedMask; sbWrite_ = info.storageBufferWriteMask;
        siUsed_ = info.storageImageUsedMask; siWrite_ = info.storageImageWriteMask;
        const VkPhysicalDeviceLimits& L = core_->props.limits;
        for (int i = 0; i < 3; ++i)
            if (local_[i] > L.maxComputeWorkGroupSize[i]) throw std::invalid_argument("Vulkan: local workgroup size exceeds maxComputeWorkGroupSize");
        if (static_cast<std::uint64_t>(local_[0]) * local_[1] * local_[2] > L.maxComputeWorkGroupInvocations)
            throw std::invalid_argument("Vulkan: local workgroup size exceeds maxComputeWorkGroupInvocations");

        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shader_->comp(), "main", nullptr};
        ci.layout = core_->pipelineLayout;
        VK_CHECK(vkCreateComputePipelines(core_->device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline_));
    }
    ~ComputePipeline() override { Core* c = core_.get(); VkPipeline p = pipeline_; c->retire([c, p] { vkDestroyPipeline(c->device, p, nullptr); }); }
    void localSize(std::uint32_t out[3]) const noexcept override { out[0] = local_[0]; out[1] = local_[1]; out[2] = local_[2]; }
    VkPipeline handle() const noexcept { return pipeline_; }
    std::uint32_t storageBufferUsedMask() const noexcept { return sbUsed_; }    // set-3 bindings the shader declares (bit = binding)
    std::uint32_t storageBufferWriteMask() const noexcept { return sbWrite_; }  // subset it may write
    std::uint32_t storageImageUsedMask() const noexcept { return siUsed_; }     // set-2 bindings (bit = binding)
    std::uint32_t storageImageWriteMask() const noexcept { return siWrite_; }
private:
    std::shared_ptr<Core> core_;
    std::shared_ptr<Shader> shader_;   // keeps the module alive for as long as the pipeline object exists
    std::uint32_t local_[3] = {1, 1, 1};
    std::uint32_t sbUsed_ = 0xFFFFFFFFu, sbWrite_ = 0xFFFFFFFFu, siUsed_ = 0xFFFFFFFFu, siWrite_ = 0xFFFFFFFFu;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

// ---------------------------------------------------------------------------------------------------------------------
class Swapchain final : public rhi::Swapchain {
public:
    Swapchain(std::shared_ptr<Core> core, const SwapchainDesc& d) : core_(std::move(core)), desc_(d), wantW_(d.width), wantH_(d.height) {
        if (!core_->surface) throw std::runtime_error("Vulkan: createSwapchain needs DeviceCreateInfo::nativeWindow (GLFWwindow*) at device creation");
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (auto& s : acquireSem_) VK_CHECK(vkCreateSemaphore(core_->device, &si, nullptr, &s));
        needRecreate_ = !create();
    }
    ~Swapchain() override {
        vkDeviceWaitIdle(core_->device);
        textures_.clear();
        for (auto s : renderFinished_) vkDestroySemaphore(core_->device, s, nullptr);
        for (auto s : acquireSem_) vkDestroySemaphore(core_->device, s, nullptr);
        if (sc_) vkDestroySwapchainKHR(core_->device, sc_, nullptr);
    }
    void resize(int w, int h) override { wantW_ = w; wantH_ = h; needRecreate_ = true; }
    TextureFormat format() const noexcept override { return fmt_; }
    int width() const noexcept override { return static_cast<int>(extent_.width); }
    int height() const noexcept override { return static_cast<int>(extent_.height); }

    std::shared_ptr<rhi::Texture> acquire() override {
        if (needRecreate_) { if (!create()) return {}; needRecreate_ = false; }
        if (extent_.width == 0 || extent_.height == 0) { needRecreate_ = true; return {}; }
        const VkResult r = vkAcquireNextImageKHR(core_->device, sc_, UINT64_MAX, acquireSem_[core_->frame], VK_NULL_HANDLE, &imageIndex_);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) { needRecreate_ = true; return {}; }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) VK_CHECK(r);
        if (r == VK_SUBOPTIMAL_KHR) needRecreate_ = true;   // keep rendering this frame, rebuild next time
        acquirePending_ = true; presentSignaled_ = false;
        textures_[imageIndex_]->layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        return textures_[imageIndex_];
    }
    void present() override {
        if (!acquirePending_ && !presentSignaled_) return;   // nothing acquired
        if (!presentSignaled_) { needRecreate_ = true; return; }   // acquired but never rendered/submitted
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &renderFinished_[imageIndex_];
        pi.swapchainCount = 1; pi.pSwapchains = &sc_; pi.pImageIndices = &imageIndex_;
        const VkResult r = vkQueuePresentKHR(core_->queue, &pi);
        presentSignaled_ = false; acquirePending_ = false;
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) needRecreate_ = true;
        else VK_CHECK(r);
    }

    // Used by Device::submit.
    bool consumeAcquire(VkSemaphore& s) { if (!acquirePending_) return false; acquirePending_ = false; s = acquireSem_[core_->frame]; return true; }
    bool consumeRenderFinished(VkSemaphore& s) { if (presentSignaled_) return false; presentSignaled_ = true; s = renderFinished_[imageIndex_]; return true; }

private:
    // Returns false while the surface has zero size (minimised); keeps the old swapchain in that case.
    bool create() {
        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(core_->phys, core_->surface, &caps));
        VkExtent2D ext = caps.currentExtent;
        if (ext.width == UINT32_MAX) {
            ext.width = std::clamp(static_cast<std::uint32_t>(std::max(0, wantW_)), caps.minImageExtent.width, caps.maxImageExtent.width);
            ext.height = std::clamp(static_cast<std::uint32_t>(std::max(0, wantH_)), caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (ext.width == 0 || ext.height == 0) { extent_ = ext; return false; }

        std::uint32_t n = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(core_->phys, core_->surface, &n, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(n);
        vkGetPhysicalDeviceSurfaceFormatsKHR(core_->phys, core_->surface, &n, formats.data());
        if (formats.empty()) throw std::runtime_error("Vulkan: surface has no formats");
        VkSurfaceFormatKHR chosen = formats[0];
        bool found = false;
        if (desc_.hdr10)
            for (auto& f : formats) if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) { chosen = f; found = true; break; }
        if (!found)
            for (auto& f : formats) if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; found = true; break; }
        if (!found)
            for (auto& f : formats) if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }

        vkGetPhysicalDeviceSurfacePresentModesKHR(core_->phys, core_->surface, &n, nullptr);
        std::vector<VkPresentModeKHR> modes(n);
        vkGetPhysicalDeviceSurfacePresentModesKHR(core_->phys, core_->surface, &n, modes.data());
        VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
        if (!desc_.vsync) {
            for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) mode = m;
            if (mode == VK_PRESENT_MODE_FIFO_KHR) for (auto m : modes) if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) mode = m;
        }

        std::uint32_t count = std::max(desc_.imageCount, caps.minImageCount);
        if (caps.maxImageCount > 0) count = std::min(count, caps.maxImageCount);

        VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        ci.surface = core_->surface; ci.minImageCount = count;
        ci.imageFormat = chosen.format; ci.imageColorSpace = chosen.colorSpace; ci.imageExtent = ext; ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                        (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        ci.presentMode = mode; ci.clipped = VK_TRUE; ci.oldSwapchain = sc_;

        vkDeviceWaitIdle(core_->device);
        VkSwapchainKHR fresh{};
        VK_CHECK(vkCreateSwapchainKHR(core_->device, &ci, nullptr, &fresh));
        textures_.clear();
        for (auto s : renderFinished_) vkDestroySemaphore(core_->device, s, nullptr);
        renderFinished_.clear();
        if (sc_) vkDestroySwapchainKHR(core_->device, sc_, nullptr);
        sc_ = fresh;

        vkGetSwapchainImagesKHR(core_->device, sc_, &n, nullptr);
        std::vector<VkImage> images(n);
        vkGetSwapchainImagesKHR(core_->device, sc_, &n, images.data());
        extent_ = ext; fmt_ = fromVkSurfaceFormat(chosen.format);
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (auto img : images) {
            textures_.push_back(std::make_shared<Texture>(core_, img, chosen.format, fmt_, static_cast<int>(ext.width), static_cast<int>(ext.height), this));
            VkSemaphore s{};
            VK_CHECK(vkCreateSemaphore(core_->device, &si, nullptr, &s));
            renderFinished_.push_back(s);
        }
        acquirePending_ = false; presentSignaled_ = false;
        return true;
    }

    std::shared_ptr<Core> core_;
    SwapchainDesc desc_;
    int wantW_ = 0, wantH_ = 0;
    VkSwapchainKHR sc_ = VK_NULL_HANDLE;
    VkExtent2D extent_{0, 0};
    TextureFormat fmt_ = TextureFormat::BGRA8;
    std::vector<std::shared_ptr<Texture>> textures_;
    std::vector<VkSemaphore> renderFinished_;
    VkSemaphore acquireSem_[kFrames] = {};
    std::uint32_t imageIndex_ = 0;
    bool acquirePending_ = false, presentSignaled_ = false, needRecreate_ = false;
};

// ---------------------------------------------------------------------------------------------------------------------
class CommandList final : public rhi::CommandList {
public:
    explicit CommandList(std::shared_ptr<Core> core) : core_(std::move(core)) {
        for (std::uint32_t i = 0; i < kFrames; ++i) {
            VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pi.queueFamilyIndex = core_->queueFamily;
            VK_CHECK(vkCreateCommandPool(core_->device, &pi, nullptr, &pools_[i]));
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.commandPool = pools_[i]; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
            VK_CHECK(vkAllocateCommandBuffers(core_->device, &ai, &bufs_[i]));
        }
    }
    ~CommandList() override {
        Core* c = core_.get();
        VkCommandPool p0 = pools_[0], p1 = pools_[1];
        keep_.clear();
        c->retire([c, p0, p1] { vkDestroyCommandPool(c->device, p0, nullptr); vkDestroyCommandPool(c->device, p1, nullptr); });
    }

    void begin() override {
        const std::uint32_t f = core_->frame;
        VK_CHECK(vkResetCommandPool(core_->device, pools_[f], 0));
        cmd_ = bufs_[f];
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd_, &bi));
        recording_ = true; inPass_ = false;
        if (core_->timestamps && !core_->queryReset[f]) {   // outside any render pass; the first list begun each frame owns the reset
            vkCmdResetQueryPool(cmd_, core_->queryPool[f], 0, Core::kMaxTimedScopes * 2);
            core_->queryReset[f] = true;
        }
        pipeline_.reset(); keep_.clear(); swapTex_.clear(); swapchain_ = nullptr;
        for (auto& b : ubo_) b.reset();
        for (auto& t : tex_) t.reset();
        for (auto& s : smp_) s.reset();
        uboOff_.fill(0); uboRange_.fill(0);
        dirtyUbo_ = dirtyTex_ = true; uboSet_ = texSet_ = VK_NULL_HANDLE;
        cpipeline_.reset();
        for (auto& x : stor_) x = {};
        for (auto& x : sbuf_) x = {};
        open_.clear(); openImg_.clear(); openWrite_ = false;
        dirtyStorImg_ = dirtyStorBuf_ = true; storImgSet_ = storBufSet_ = VK_NULL_HANDLE;
        boundGfx_[0] = boundGfx_[1] = VK_NULL_HANDLE;
        for (auto& x : boundCmp_) x = VK_NULL_HANDLE;
    }
    void end() override {
        requireRecording();
        if (inPass_) endRenderPass();
        flushBufferAccesses(true);   // later command lists / submissions must see (and not race with) this list's storage-buffer accesses
        if (core_->timestamps) while (core_->openDepth[core_->frame] > 0) endTimedScope();   // unbalanced push/pop: still produce complete pairs
        if (!swapTex_.empty()) {
            std::vector<VkImageMemoryBarrier2> bars;
            for (auto& t : swapTex_) {
                bars.push_back(makeBarrier(t->image_, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, t->layout_, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                           scopeFor(t->layout_), scopeFor(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)));
                t->layout_ = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            core_->barriers(cmd_, bars.data(), static_cast<std::uint32_t>(bars.size()));
        }
        VK_CHECK(vkEndCommandBuffer(cmd_));
        recording_ = false;
    }

    void setPipeline(const std::shared_ptr<rhi::Pipeline>& p) override {
        requireRecording();
        pipeline_ = std::static_pointer_cast<Pipeline>(p);
        if (!pipeline_) throw std::invalid_argument("Vulkan: setPipeline(null)");
        vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->handle());
        keep_.push_back(pipeline_);
    }
    void bindVertexBuffer(const std::shared_ptr<rhi::Buffer>& b, unsigned binding) override {
        requireRecording();
        auto vb = std::static_pointer_cast<Buffer>(b);
        vb->sync();
        const VkBuffer h = vb->handle(); const VkDeviceSize off = vb->slotOffset();
        vkCmdBindVertexBuffers(cmd_, binding, 1, &h, &off);
        keep_.push_back(vb);
    }
    void bindIndexBuffer(const std::shared_ptr<rhi::Buffer>& b) override {
        requireRecording();
        auto ib = std::static_pointer_cast<Buffer>(b);
        ib->sync();
        vkCmdBindIndexBuffer(cmd_, ib->handle(), ib->slotOffset(), VK_INDEX_TYPE_UINT32);
        keep_.push_back(ib);
    }
    struct BoundView { VkImageView view = VK_NULL_HANDLE; std::uint32_t m0 = 0, mc = 1, l0 = 0, lc = 1; };
    static bool overlap(std::uint32_t a, std::uint32_t an, std::uint32_t b, std::uint32_t bn) { return a < b + bn && b < a + an; }
    static VkImageViewType naturalViewType(TextureDimension d) {
        switch (d) {
            case TextureDimension::Cube: return VK_IMAGE_VIEW_TYPE_CUBE;
            case TextureDimension::Tex2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case TextureDimension::CubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            case TextureDimension::Tex3D: return VK_IMAGE_VIEW_TYPE_3D;
            default: return VK_IMAGE_VIEW_TYPE_2D;
        }
    }
    BoundView resolveView(Texture& t, const TextureViewDesc& d) {
        const std::uint32_t L = static_cast<std::uint32_t>(t.levels_), Y = static_cast<std::uint32_t>(t.layers_);
        BoundView b;
        b.m0 = d.baseMip; b.mc = d.mipCount ? d.mipCount : (L > b.m0 ? L - b.m0 : 0);
        b.l0 = d.baseLayer; b.lc = d.layerCount ? d.layerCount : (Y > b.l0 ? Y - b.l0 : 0);
        if (b.mc == 0 || b.m0 + b.mc > L || b.lc == 0 || b.l0 + b.lc > Y) throw std::out_of_range("Vulkan: texture view range is outside the texture");
        const bool cubeish = t.dim_ == TextureDimension::Cube || t.dim_ == TextureDimension::CubeArray;
        VkImageViewType vt = VK_IMAGE_VIEW_TYPE_2D;
        switch (d.type) {
            case ViewType::Auto:
                if (t.dim_ == TextureDimension::Tex3D || (b.l0 == 0 && b.lc == Y)) vt = naturalViewType(t.dim_);
                else if (b.lc == 1) vt = VK_IMAGE_VIEW_TYPE_2D;
                else if (cubeish && b.l0 % 6 == 0 && b.lc == 6) vt = VK_IMAGE_VIEW_TYPE_CUBE;
                else if (cubeish && b.l0 % 6 == 0 && b.lc % 6 == 0) vt = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                else vt = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                break;
            case ViewType::Tex2D: if (b.lc != 1) throw std::invalid_argument("Vulkan: a 2D view covers exactly one layer"); vt = VK_IMAGE_VIEW_TYPE_2D; break;
            case ViewType::Tex2DArray: vt = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
            case ViewType::Cube: if (b.lc != 6) throw std::invalid_argument("Vulkan: a cube view covers exactly 6 layers"); vt = VK_IMAGE_VIEW_TYPE_CUBE; break;
            case ViewType::CubeArray:
                if (b.lc % 6 != 0 || !core_->cubeArray) throw std::invalid_argument("Vulkan: cube array view needs a multiple of 6 layers and imageCubeArray");
                vt = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; break;
            case ViewType::Tex3D: if (t.dim_ != TextureDimension::Tex3D) throw std::invalid_argument("Vulkan: 3D view of a non-3D texture"); vt = VK_IMAGE_VIEW_TYPE_3D; break;
        }
        if (vt == naturalViewType(t.dim_) && b.m0 == 0 && b.mc == L && b.l0 == 0 && b.lc == Y) b.view = t.sampleView_;
        else b.view = t.getView(vt, sampleAspect(t.fmt_), b.m0, b.mc, b.l0, b.lc);
        return b;
    }
    void bindTexture(unsigned unit, const std::shared_ptr<rhi::Texture>& t, const std::shared_ptr<rhi::Sampler>& s) override {
        bindTextureView(unit, t, TextureViewDesc{}, s);
    }
    void bindTextureView(unsigned unit, const std::shared_ptr<rhi::Texture>& t, const TextureViewDesc& vd, const std::shared_ptr<rhi::Sampler>& s) override {
        requireRecording();
        if (unit >= kMaxTextureSlots) throw std::out_of_range("Vulkan: texture slot out of range");
        auto vt = std::static_pointer_cast<Texture>(t);
        if (vt) { bv_[unit] = resolveView(*vt, vd); ensureReadable(*vt, bv_[unit]); }
        tex_[unit] = vt; smp_[unit] = std::static_pointer_cast<Sampler>(s);
        if (vt) keep_.push_back(vt);
        if (smp_[unit]) keep_.push_back(smp_[unit]);
        dirtyTex_ = true;
    }
    void draw(std::uint32_t vertexCount, std::uint32_t firstVertex) override { drawInstanced(vertexCount, 1, firstVertex, 0); }
    void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex) override { drawIndexedInstanced(indexCount, 1, firstIndex, 0, 0); }

    void beginRenderPass(const RenderPassDesc& d) override {
        requireRecording();
        if (inPass_) throw std::logic_error("Vulkan: beginRenderPass inside a render pass");
        flushBufferAccesses(false);   // compute-written buffers become visible to vertex / index / indirect / shader reads in the pass
        pass_ = d;
        labelDepth_ = !d.label.empty();
        if (labelDepth_) pushDebugGroup(d.label.c_str());   // before beginRendering: the timed scope includes clears and layout transitions
        beginRendering(pass_, false);
        dirtyTex_ = true;   // bound textures may have changed layout since the descriptor set was written
    }
    void endRenderPass() override {
        requireRecording();
        if (!inPass_) throw std::logic_error("Vulkan: endRenderPass outside a render pass");
        core_->cmdEndRendering(cmd_);
        inPass_ = false;
        if (labelDepth_) { popDebugGroup(); labelDepth_ = false; }   // after endRendering: includes store ops
    }
    void setViewport(float x, float y, float w, float h, float minD, float maxD) override {
        requireRecording();
        const VkViewport vp{x, y + h, w, -h, minD, maxD};   // flipped: GL-style clip space / winding
        vkCmdSetViewport(cmd_, 0, 1, &vp);
    }
    void setScissor(int x, int y, std::uint32_t w, std::uint32_t h) override {
        requireRecording();
        const VkRect2D r{{x, y}, {w, h}};
        vkCmdSetScissor(cmd_, 0, 1, &r);
    }
    void bindUniformBuffer(unsigned slot, const std::shared_ptr<rhi::Buffer>& b, std::size_t offset, std::size_t range) override {
        requireRecording();
        if (slot >= kMaxUniformSlots) throw std::out_of_range("Vulkan: uniform slot out of range");
        if (offset % core_->props.limits.minUniformBufferOffsetAlignment != 0)
            throw std::invalid_argument("Vulkan: uniform buffer offset must be a multiple of minUniformBufferOffsetAlignment");
        ubo_[slot] = std::static_pointer_cast<Buffer>(b);
        uboOff_[slot] = offset; uboRange_[slot] = range;
        if (ubo_[slot]) keep_.push_back(ubo_[slot]);
        dirtyUbo_ = true;
    }
    void pushConstants(const void* data, std::uint32_t size, std::uint32_t offset) override {
        requireRecording();
        if (offset + size > kMaxPushConstantBytes || (size % 4) || (offset % 4)) throw std::invalid_argument("Vulkan: invalid push constant range");
        vkCmdPushConstants(cmd_, core_->pipelineLayout, kPushStages, offset, size, data);
    }
    void drawInstanced(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex, std::uint32_t firstInstance) override {
        prepareDraw();
        vkCmdDraw(cmd_, vertexCount, instanceCount, firstVertex, firstInstance);
    }
    void drawIndexedInstanced(std::uint32_t indexCount, std::uint32_t instanceCount, std::uint32_t firstIndex, std::int32_t baseVertex, std::uint32_t firstInstance) override {
        prepareDraw();
        vkCmdDrawIndexed(cmd_, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }
    // ---- v3 compute ----------------------------------------------------------------------------------------------------
    void requireCompute() const {
        if (!core_->caps.compute) throw UnsupportedByBackend("Vulkan: compute is not available on this device");
    }
    // Storage image view rule: Tex3D -> 3D view of the mip; layerCount == 1 -> 2D view of that layer (image2D); otherwise 2D array
    // view (image2DArray; a cube is addressed as 6 layers). layerCount 0 = all remaining layers. Needs TextureUsage::Storage.
    void bindStorageTexture(unsigned slot, const std::shared_ptr<rhi::Texture>& t, std::uint32_t mip, std::uint32_t baseLayer, std::uint32_t layerCount) override {
        requireRecording(); requireCompute();
        if (slot >= kMaxStorageImageSlots) throw std::out_of_range("Vulkan: storage image slot out of range");
        auto vt = std::static_pointer_cast<Texture>(t);
        if (!vt) { stor_[slot] = {}; dirtyStorImg_ = true; return; }
        if (!hasUsage(vt->usage_, TextureUsage::Storage)) throw std::invalid_argument("Vulkan: bindStorageTexture needs a texture created with TextureUsage::Storage");
        const std::uint32_t L = static_cast<std::uint32_t>(vt->levels_), Y = static_cast<std::uint32_t>(vt->layers_);
        if (mip >= L) throw std::out_of_range("Vulkan: storage image mip is outside the texture");
        StorageImage si; si.tex = vt; si.m0 = mip;
        VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
        if (vt->dim_ == TextureDimension::Tex3D) {
            if (baseLayer != 0) throw std::invalid_argument("Vulkan: 3D storage images are bound whole (baseLayer must be 0)");
            type = VK_IMAGE_VIEW_TYPE_3D; si.l0 = 0; si.lc = 1;
        } else {
            const std::uint32_t lc = layerCount ? layerCount : (Y > baseLayer ? Y - baseLayer : 0);
            if (lc == 0 || baseLayer + lc > Y) throw std::out_of_range("Vulkan: storage image layer range is outside the texture");
            si.l0 = baseLayer; si.lc = lc;
            type = lc == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        si.view = vt->getView(type, VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, si.l0, si.lc);
        stor_[slot] = si;
        keep_.push_back(vt);
        dirtyStorImg_ = true;
    }
    // Buffers must be created with BufferUsage::Storage or Indirect. `dynamic` buffers are CPU-updated per-frame rings: treat them as read-only in the shader.
    void bindStorageBuffer(unsigned slot, const std::shared_ptr<rhi::Buffer>& b, std::size_t offset, std::size_t range) override {
        requireRecording(); requireCompute();
        if (slot >= kMaxStorageBufferSlots) throw std::out_of_range("Vulkan: storage buffer slot out of range");
        auto vb = std::static_pointer_cast<Buffer>(b);
        if (!vb) { sbuf_[slot] = {}; dirtyStorBuf_ = true; return; }
        if (vb->usage() != BufferUsage::Storage && vb->usage() != BufferUsage::Indirect)
            throw std::invalid_argument("Vulkan: bindStorageBuffer needs a buffer created with BufferUsage::Storage (or Indirect)");
        const VkPhysicalDeviceLimits& lim = core_->props.limits;
        if (offset % lim.minStorageBufferOffsetAlignment != 0) throw std::invalid_argument("Vulkan: storage buffer offset must be a multiple of minStorageBufferOffsetAlignment");
        if (offset >= vb->size()) throw std::out_of_range("Vulkan: storage buffer offset is outside the buffer");
        const std::size_t r = range ? range : vb->size() - offset;
        if (offset + r > vb->size()) throw std::out_of_range("Vulkan: storage buffer range is outside the buffer");
        if (r > lim.maxStorageBufferRange) throw std::invalid_argument("Vulkan: storage buffer range exceeds maxStorageBufferRange");
        sbuf_[slot] = {vb, offset, r};
        keep_.push_back(vb);
        dirtyStorBuf_ = true;
    }
    void setComputePipeline(const std::shared_ptr<rhi::ComputePipeline>& p) override {
        requireRecording(); requireCompute();
        cpipeline_ = std::static_pointer_cast<ComputePipeline>(p);
        if (!cpipeline_) throw std::invalid_argument("Vulkan: setComputePipeline(null)");
        vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, cpipeline_->handle());
        keep_.push_back(cpipeline_);
    }
    void dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z) override {
        requireRecording(); requireCompute();
        const VkPhysicalDeviceLimits& lim = core_->props.limits;
        if (x > lim.maxComputeWorkGroupCount[0] || y > lim.maxComputeWorkGroupCount[1] || z > lim.maxComputeWorkGroupCount[2])
            throw std::invalid_argument("Vulkan: dispatch exceeds maxComputeWorkGroupCount");
        if (x == 0 || y == 0 || z == 0) return;
        prepareDispatch(nullptr, 0);
        vkCmdDispatch(cmd_, x, y, z);
        finishDispatch();
    }
    void dispatchIndirect(const std::shared_ptr<rhi::Buffer>& b, std::size_t offset) override {
        requireRecording(); requireCompute();
        auto ib = std::static_pointer_cast<Buffer>(b);
        if (!ib || ib->usage() != BufferUsage::Indirect) throw std::invalid_argument("Vulkan: dispatchIndirect needs a buffer created with BufferUsage::Indirect");
        if (offset % 4 != 0 || offset + 3 * sizeof(std::uint32_t) > ib->size()) throw std::out_of_range("Vulkan: dispatchIndirect offset is misaligned or outside the buffer");
        prepareDispatch(ib, offset);
        ib->sync();
        vkCmdDispatchIndirect(cmd_, ib->handle(), ib->slotOffset() + offset);
        keep_.push_back(ib);
        finishDispatch();
    }
    void generateMipmaps(const std::shared_ptr<rhi::Texture>& tex) override {
        auto t = std::static_pointer_cast<Texture>(tex);
        if (t) generateMipmapsRange(tex, 0, static_cast<std::uint32_t>(t->layers_));
    }
    // Blit-generates mips 1.. from mip 0 for layers [baseLayer, baseLayer+layerCount) (and every slice of a volume).
    void generateMipmapsRange(const std::shared_ptr<rhi::Texture>& tex, std::uint32_t baseLayer, std::uint32_t layerCount) override {
        requireRecording();
        if (inPass_) throw std::logic_error("Vulkan: generateMipmaps inside a render pass");
        auto t = std::static_pointer_cast<Texture>(tex);
        if (!t || t->levels_ <= 1) return;
        if (isDepthFormat(t->fmt_)) throw std::invalid_argument("Vulkan: generateMipmaps on a depth texture");
        if (isCompressed(t->fmt_)) throw std::invalid_argument("Vulkan: generateMipmaps on a block-compressed texture");
        const std::uint32_t Y = static_cast<std::uint32_t>(t->layers_);
        if (layerCount == 0) layerCount = Y > baseLayer ? Y - baseLayer : 0;
        if (layerCount == 0 || baseLayer + layerCount > Y) throw std::out_of_range("Vulkan: generateMipmaps layer range is outside the texture");
        const auto A = VK_IMAGE_ASPECT_COLOR_BIT;
        const std::uint32_t levels = static_cast<std::uint32_t>(t->levels_);
        {
            std::vector<VkImageMemoryBarrier2> b;
            t->collect(b, A, 0, 1, baseLayer, layerCount, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false);
            t->collect(b, A, 1, levels - 1, baseLayer, layerCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
            core_->barriers(cmd_, b.data(), static_cast<std::uint32_t>(b.size()));
        }
        int mw = t->w_, mh = t->h_, md = t->d_;
        for (std::uint32_t i = 1; i < levels; ++i) {
            const int nw = std::max(1, mw / 2), nh = std::max(1, mh / 2), nd = std::max(1, md / 2);
            VkImageBlit blit{};
            blit.srcSubresource = {A, i - 1, baseLayer, layerCount}; blit.srcOffsets[1] = {mw, mh, md};
            blit.dstSubresource = {A, i, baseLayer, layerCount}; blit.dstOffsets[1] = {nw, nh, nd};
            vkCmdBlitImage(cmd_, t->image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t->image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            t->transition(cmd_, A, i, 1, baseLayer, layerCount, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false);
            mw = nw; mh = nh; md = nd;
        }
        t->transition(cmd_, A, 0, levels, baseLayer, layerCount, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
        keep_.push_back(t);
        dirtyTex_ = true;
    }
    // Same-format copy of one subresource (mip + layer) to another; extents must match. Needs TransferSrc / TransferDst usage on the textures.
    void copyTexture(const std::shared_ptr<rhi::Texture>& src, std::uint32_t sm, std::uint32_t sl,
                     const std::shared_ptr<rhi::Texture>& dst, std::uint32_t dm, std::uint32_t dl) override {
        requireRecording();
        if (inPass_) throw std::logic_error("Vulkan: copyTexture inside a render pass");
        auto s = std::static_pointer_cast<Texture>(src); auto d = std::static_pointer_cast<Texture>(dst);
        if (!s || !d) throw std::invalid_argument("Vulkan: copyTexture needs a source and a destination");
        if (s->fmt_ != d->fmt_) throw std::invalid_argument("Vulkan: copyTexture needs identical formats");
        if (sm >= static_cast<std::uint32_t>(s->levels_) || sl >= static_cast<std::uint32_t>(s->layers_) ||
            dm >= static_cast<std::uint32_t>(d->levels_) || dl >= static_cast<std::uint32_t>(d->layers_)) throw std::out_of_range("Vulkan: copyTexture subresource out of range");
        const int w = std::max(1, s->w_ >> sm), h = std::max(1, s->h_ >> sm), dp = std::max(1, s->d_ >> sm);
        if (w != std::max(1, d->w_ >> dm) || h != std::max(1, d->h_ >> dm) || dp != std::max(1, d->d_ >> dm)) throw std::invalid_argument("Vulkan: copyTexture extents differ");
        if (s == d && sm == dm && sl == dl) throw std::invalid_argument("Vulkan: copyTexture source and destination are the same subresource");
        const VkImageAspectFlags A = attachAspect(s->fmt_);
        std::vector<VkImageMemoryBarrier2> b;
        s->collect(b, A, sm, 1, sl, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false);
        d->collect(b, A, dm, 1, dl, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);   // fully overwritten
        if (!b.empty()) core_->barriers(cmd_, b.data(), static_cast<std::uint32_t>(b.size()));
        VkImageCopy rg{};
        rg.srcSubresource = {A, sm, sl, 1}; rg.dstSubresource = {A, dm, dl, 1};
        rg.extent = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), static_cast<std::uint32_t>(dp)};
        vkCmdCopyImage(cmd_, s->image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d->image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rg);
        keep_.push_back(s); keep_.push_back(d);
        dirtyTex_ = true;
    }
    void pushDebugGroup(const char* name) override {
        if (!recording_) return;
        if (core_->cmdBeginLabel) {
            VkDebugUtilsLabelEXT l{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT}; l.pLabelName = name;
            core_->cmdBeginLabel(cmd_, &l);
        }
        beginTimedScope(name);
    }
    void popDebugGroup() override {
        if (!recording_) return;
        if (core_->cmdEndLabel) core_->cmdEndLabel(cmd_);
        endTimedScope();
    }

    VkCommandBuffer handle() const noexcept { return cmd_; }
    Swapchain* swapchain() const noexcept { return swapchain_; }

private:
    void requireRecording() const { if (!recording_) throw std::logic_error("Vulkan: CommandList::begin() was not called"); }

    void writeTimestamp(std::uint32_t query) {
        core_->cmdWriteTimestamp2(cmd_, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, core_->queryPool[core_->frame], query);
    }
    void beginTimedScope(const char* name) {
        Core& c = *core_;
        if (!c.timestamps) return;
        const std::uint32_t f = c.frame;
        const std::uint32_t d = c.openDepth[f];
        if (d >= c.openScopes[f].size()) return;   // nesting deeper than the tracker: untimed and unbalanced-safe (see endTimedScope)
        if (c.scopeCount[f] >= Core::kMaxTimedScopes) { c.openScopes[f][d] = Core::kNoScope; ++c.openDepth[f]; return; }
        const std::uint32_t idx = c.scopeCount[f]++;
        Core::TimedScope& s = c.scopes[f][idx];
        std::strncpy(s.name, name ? name : "", sizeof s.name - 1);
        s.name[sizeof s.name - 1] = '\0';
        s.depth = d; s.q0 = c.queryCount[f]; s.q1 = c.queryCount[f] + 1;
        c.queryCount[f] += 2;
        c.openScopes[f][d] = idx; ++c.openDepth[f];
        writeTimestamp(s.q0);
    }
    void endTimedScope() {
        Core& c = *core_;
        if (!c.timestamps) return;
        const std::uint32_t f = c.frame;
        if (c.openDepth[f] == 0) return;
        const std::uint32_t idx = c.openScopes[f][--c.openDepth[f]];
        if (idx != Core::kNoScope) writeTimestamp(c.scopes[f][idx].q1);
    }

    // Puts a subresource range into its sampled layout. Inside a render pass this splits the pass (end, barrier, resume with Load).
    void ensureReadable(Texture& t, const BoundView& r) {
        const VkImageLayout want = readLayout(t.fmt_);
        if (t.allIn(want, r.m0, r.mc, r.l0, r.lc)) return;
        if (inPass_) {
            for (auto& c : pass_.colors)
                if (c.texture.get() == &t && overlap(c.mip, 1, r.m0, r.mc) && overlap(c.layer, std::max(1u, c.layerCount), r.l0, r.lc))
                    throw std::logic_error("Vulkan: texture is sampled while it is a render target of the current pass");
            if (pass_.depth.texture.get() == &t && overlap(pass_.depth.mip, 1, r.m0, r.mc) && overlap(pass_.depth.layer, std::max(1u, pass_.depth.layerCount), r.l0, r.lc))
                throw std::logic_error("Vulkan: texture is sampled while it is the depth target of the current pass");
            core_->cmdEndRendering(cmd_);
            t.transition(cmd_, attachAspect(t.fmt_), r.m0, r.mc, r.l0, r.lc, want, false);
            beginRendering(pass_, true);
        } else {
            t.transition(cmd_, attachAspect(t.fmt_), r.m0, r.mc, r.l0, r.lc, want, false);
        }
    }
    void noteSwapchain(const std::shared_ptr<Texture>& t) {
        if (!t->owner_) return;
        swapchain_ = t->owner_;
        for (auto& s : swapTex_) if (s == t) return;
        swapTex_.push_back(t);
    }
    void checkAttachment(const Texture& t, std::uint32_t mip, std::uint32_t layer, std::uint32_t lc) {
        if (mip >= static_cast<std::uint32_t>(t.levels_) || layer + lc > static_cast<std::uint32_t>(t.layers_))
            throw std::out_of_range("Vulkan: render pass attachment mip/layer is outside the texture");
        if (lc > 1 && !core_->caps.shaderOutputLayer)
            throw UnsupportedByBackend("Vulkan: layerCount > 1 needs shaderOutputLayer (render the faces one by one instead)");
    }
    void beginRendering(const RenderPassDesc& d, bool resume) {
        std::vector<VkImageMemoryBarrier2> bars;
        VkRenderingAttachmentInfo colors[kMaxColorAttachments]{};
        std::uint32_t n = 0, w = 0, h = 0, layerCount = 0;
        auto noteLayers = [&](std::uint32_t lc) {
            if (layerCount && layerCount != lc) throw std::invalid_argument("Vulkan: all attachments of a pass need the same layerCount");
            layerCount = lc;
        };
        for (auto& c : d.colors) {
            if (!c.texture) throw std::invalid_argument("Vulkan: render pass colour attachment without texture");
            if (n >= kMaxColorAttachments) throw std::invalid_argument("Vulkan: too many colour attachments");
            auto t = std::static_pointer_cast<Texture>(c.texture);
            const std::uint32_t lc = std::max(1u, c.layerCount);
            checkAttachment(*t, c.mip, c.layer, lc);
            noteLayers(lc);
            const bool load = resume || c.load == LoadOp::Load;
            t->collect(bars, VK_IMAGE_ASPECT_COLOR_BIT, c.mip, 1, c.layer, lc, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, !load);
            noteSwapchain(t);
            if (!resume) keep_.push_back(t);
            colors[n].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colors[n].imageView = t->getView(lc > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, c.mip, 1, c.layer, lc);
            colors[n].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colors[n].loadOp = load ? VK_ATTACHMENT_LOAD_OP_LOAD : (c.load == LoadOp::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
            colors[n].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // DontCare is ignored on purpose: passes can be split (see ensureReadable)
            std::memcpy(colors[n].clearValue.color.float32, c.clear, sizeof(float) * 4);
            const std::uint32_t cw = static_cast<std::uint32_t>(std::max(1, t->w_ >> c.mip)), ch = static_cast<std::uint32_t>(std::max(1, t->h_ >> c.mip));
            if (n == 0) { w = cw; h = ch; }
            ++n;
        }
        VkRenderingAttachmentInfo depthInfo{}; bool hasDepth = false, stencil = false;
        if (d.depth.texture) {
            auto t = std::static_pointer_cast<Texture>(d.depth.texture);
            const std::uint32_t lc = std::max(1u, d.depth.layerCount);
            checkAttachment(*t, d.depth.mip, d.depth.layer, lc);
            noteLayers(lc);
            const bool load = resume || d.depth.load == LoadOp::Load;
            t->collect(bars, attachAspect(t->fmt_), d.depth.mip, 1, d.depth.layer, lc, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, !load);
            if (!resume) keep_.push_back(t);
            depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthInfo.imageView = t->getView(lc > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, attachAspect(t->fmt_), d.depth.mip, 1, d.depth.layer, lc);
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthInfo.loadOp = load ? VK_ATTACHMENT_LOAD_OP_LOAD : (d.depth.load == LoadOp::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
            depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthInfo.clearValue.depthStencil = {d.depth.clearDepth, 0};
            hasDepth = true; stencil = hasStencil(t->fmt_);
            if (n == 0) { w = static_cast<std::uint32_t>(std::max(1, t->w_ >> d.depth.mip)); h = static_cast<std::uint32_t>(std::max(1, t->h_ >> d.depth.mip)); }
        }
        if (n == 0 && !hasDepth) throw std::invalid_argument("Vulkan: render pass without attachments");
        if (!bars.empty()) core_->barriers(cmd_, bars.data(), static_cast<std::uint32_t>(bars.size()));

        VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
        ri.renderArea = {{0, 0}, {w, h}}; ri.layerCount = layerCount ? layerCount : 1;
        ri.colorAttachmentCount = n; ri.pColorAttachments = n ? colors : nullptr;
        ri.pDepthAttachment = hasDepth ? &depthInfo : nullptr;
        ri.pStencilAttachment = stencil ? &depthInfo : nullptr;
        core_->cmdBeginRendering(cmd_, &ri);
        inPass_ = true;
        if (!resume) {
            const VkViewport vp{0.0f, static_cast<float>(h), static_cast<float>(w), -static_cast<float>(h), 0.0f, 1.0f};
            const VkRect2D sc{{0, 0}, {w, h}};
            vkCmdSetViewport(cmd_, 0, 1, &vp);
            vkCmdSetScissor(cmd_, 0, 1, &sc);
        }
    }

    // Rewrites sets 0 / 1 when they are out of date (may split a render pass: sampled textures are moved to their read layout first).
    void refreshSets() {
        if (dirtyTex_) {
            for (unsigned i = 0; i < kMaxTextureSlots; ++i) if (tex_[i]) ensureReadable(*tex_[i], bv_[i]);   // may split the pass
            VkDescriptorSet set = core_->allocSet(core_->texLayout);
            VkDescriptorImageInfo infos[kMaxTextureSlots]; VkWriteDescriptorSet w[kMaxTextureSlots]{};
            for (unsigned i = 0; i < kMaxTextureSlots; ++i) {
                if (tex_[i]) {
                    infos[i] = {smp_[i] ? smp_[i]->handle() : core_->defaultSampler, bv_[i].view, readLayout(tex_[i]->fmt_)};
                } else {
                    infos[i] = {core_->defaultSampler, core_->fbView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                }
                w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, i, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[i], nullptr, nullptr};
            }
            vkUpdateDescriptorSets(core_->device, kMaxTextureSlots, w, 0, nullptr);
            texSet_ = set; dirtyTex_ = false;
        }
        if (dirtyUbo_) {
            VkDescriptorSet set = core_->allocSet(core_->uboLayout);
            VkDescriptorBufferInfo infos[kMaxUniformSlots]; VkWriteDescriptorSet w[kMaxUniformSlots]{};
            for (unsigned i = 0; i < kMaxUniformSlots; ++i) {
                if (ubo_[i]) {
                    ubo_[i]->sync();
                    const VkDeviceSize range = uboRange_[i] ? uboRange_[i] : ubo_[i]->size() - uboOff_[i];
                    infos[i] = {ubo_[i]->handle(), ubo_[i]->slotOffset() + uboOff_[i], range};
                } else {
                    infos[i] = {core_->fbBuffer, 0, kFallbackUboSize};
                }
                w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, i, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &infos[i], nullptr};
            }
            vkUpdateDescriptorSets(core_->device, kMaxUniformSlots, w, 0, nullptr);
            uboSet_ = set; dirtyUbo_ = false;
        }
    }
    void prepareDraw() {
        requireRecording();
        if (!inPass_) throw std::logic_error("Vulkan: draw outside a render pass");
        if (!pipeline_) throw std::logic_error("Vulkan: draw without a pipeline");
        refreshSets();
        if (boundGfx_[0] != uboSet_ || boundGfx_[1] != texSet_) {
            const VkDescriptorSet sets[2] = {uboSet_, texSet_};
            vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, core_->pipelineLayout, 0, 2, sets, 0, nullptr);
            boundGfx_[0] = uboSet_; boundGfx_[1] = texSet_;
        }
    }
    void prepareDispatch(const std::shared_ptr<Buffer>& indirect, std::size_t indirectOffset) {
        if (inPass_) throw std::logic_error("Vulkan: dispatch inside a render pass (end the pass first)");
        if (!cpipeline_) throw std::logic_error("Vulkan: dispatch without a compute pipeline");
        // A subresource cannot be sampled (sets 0/1) and written (set 2) by the same dispatch.
        for (unsigned i = 0; i < kMaxStorageImageSlots; ++i) {
            if (!stor_[i].tex) continue;
            for (unsigned u = 0; u < kMaxTextureSlots; ++u)
                if (tex_[u] == stor_[i].tex && overlap(bv_[u].m0, bv_[u].mc, stor_[i].m0, 1) && overlap(bv_[u].l0, bv_[u].lc, stor_[i].l0, stor_[i].lc))
                    throw std::logic_error("Vulkan: texture is bound as sampled (slot " + std::to_string(u) + ") and as storage image (slot " + std::to_string(i) +
                                           ") with overlapping mips/layers; unbind the sampled slot with bindTexture(slot, nullptr)");
        }
        refreshSets();
        // Storage images -> GENERAL (layout transitions only; GENERAL -> GENERAL is a no-op here, hazards are ordered by trackAccesses).
        std::vector<VkImageMemoryBarrier2> bars;
        bool anyImg = false, anyBuf = false;
        for (auto& si : stor_) if (si.tex) { si.tex->collect(bars, VK_IMAGE_ASPECT_COLOR_BIT, si.m0, 1, si.l0, si.lc, VK_IMAGE_LAYOUT_GENERAL, false); anyImg = true; }
        if (!bars.empty()) core_->barriers(cmd_, bars.data(), static_cast<std::uint32_t>(bars.size()));

        if (boundCmp_[0] != uboSet_ || boundCmp_[1] != texSet_) {
            const VkDescriptorSet sets[2] = {uboSet_, texSet_};
            vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, core_->pipelineLayout, 0, 2, sets, 0, nullptr);
            boundCmp_[0] = uboSet_; boundCmp_[1] = texSet_;
        }
        if (anyImg) {   // set 2 (unbound slots stay unwritten: the shader must not touch them)
            if (dirtyStorImg_ || storImgSet_ == VK_NULL_HANDLE) {
                VkDescriptorSet set = core_->allocSet(core_->storImgLayout);
                VkDescriptorImageInfo infos[kMaxStorageImageSlots]; VkWriteDescriptorSet w[kMaxStorageImageSlots]{};
                std::uint32_t n = 0;
                for (unsigned i = 0; i < kMaxStorageImageSlots; ++i) {
                    if (!stor_[i].tex) continue;
                    infos[n] = {VK_NULL_HANDLE, stor_[i].view, VK_IMAGE_LAYOUT_GENERAL};
                    w[n] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, i, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &infos[n], nullptr, nullptr};
                    ++n;
                }
                vkUpdateDescriptorSets(core_->device, n, w, 0, nullptr);
                storImgSet_ = set; dirtyStorImg_ = false;
            }
            if (boundCmp_[2] != storImgSet_) {
                vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, core_->pipelineLayout, 2, 1, &storImgSet_, 0, nullptr);
                boundCmp_[2] = storImgSet_;
            }
        }
        for (auto& sb : sbuf_) if (sb.buf) anyBuf = true;
        trackAccesses(indirect, indirectOffset);
        if (anyBuf) {   // set 3 (unbound slots point at the fallback buffer)
            if (dirtyStorBuf_ || storBufSet_ == VK_NULL_HANDLE) {
                VkDescriptorSet set = core_->allocSet(core_->storBufLayout);
                VkDescriptorBufferInfo infos[kMaxStorageBufferSlots]; VkWriteDescriptorSet w[kMaxStorageBufferSlots]{};
                for (unsigned i = 0; i < kMaxStorageBufferSlots; ++i) {
                    if (sbuf_[i].buf) { sbuf_[i].buf->sync(); infos[i] = {sbuf_[i].buf->handle(), sbuf_[i].buf->slotOffset() + sbuf_[i].offset, sbuf_[i].range}; }
                    else infos[i] = {core_->fbBuffer, 0, kFallbackUboSize};
                    w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, i, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &infos[i], nullptr};
                }
                vkUpdateDescriptorSets(core_->device, kMaxStorageBufferSlots, w, 0, nullptr);
                storBufSet_ = set; dirtyStorBuf_ = false;
            }
            if (boundCmp_[3] != storBufSet_) {
                vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, core_->pipelineLayout, 3, 1, &storBufSet_, 0, nullptr);
                boundCmp_[3] = storBufSet_;
            }
        }
    }
    // ---- storage hazard tracking (buffers and images) --------------------------------------------------------------------
    // `open_` / `openImg_` hold every GPU-side storage access recorded since the last compute barrier. A new access conflicts with an open
    // one when the same buffer byte range / image mip-and-layer range overlaps and at least one of the two writes (RAW, WAR, WAW); only
    // then a barrier is emitted. Read-only sharing, disjoint ranges and different resources never synchronise. Read vs write comes from the
    // shader's NonWritable decorations (`readonly` = read). Layout transitions into GENERAL are separate image barriers (Texture::collect).
    // `dynamic` ring buffers are CPU-updated and ignored. dispatchIndirect reads 12 bytes of its argument buffer.
    struct BufAccess { VkBuffer buf; VkDeviceSize b, e; bool write; };
    struct ImgAccess { VkImage img; std::uint32_t m0, mc, l0, lc; bool write; };
    void trackAccesses(const std::shared_ptr<Buffer>& indirect, std::size_t indirectOffset) {
        std::vector<BufAccess> acc;
        std::vector<ImgAccess> iacc;
        const std::uint32_t used = cpipeline_->storageBufferUsedMask(), write = cpipeline_->storageBufferWriteMask();
        for (unsigned i = 0; i < kMaxStorageBufferSlots; ++i) {
            const StorageBuf& sb = sbuf_[i];
            if (!sb.buf || !sb.buf->gpuWritable() || !((used >> i) & 1u)) continue;
            acc.push_back({sb.buf->handle(), sb.offset, sb.offset + sb.range, ((write >> i) & 1u) != 0});
        }
        if (indirect && indirect->gpuWritable()) acc.push_back({indirect->handle(), indirectOffset, indirectOffset + 3 * sizeof(std::uint32_t), false});
        const std::uint32_t iusedM = cpipeline_->storageImageUsedMask(), iwriteM = cpipeline_->storageImageWriteMask();
        for (unsigned i = 0; i < kMaxStorageImageSlots; ++i) {
            const StorageImage& si = stor_[i];
            if (!si.tex || !((iusedM >> i) & 1u)) continue;
            iacc.push_back({si.tex->image_, si.m0, 1, si.l0, si.lc, ((iwriteM >> i) & 1u) != 0});
        }
        bool conflict = open_.size() + acc.size() + openImg_.size() + iacc.size() > kMaxOpenAccesses;   // bounded bookkeeping: past the cap just synchronise
        for (const BufAccess& n : acc) {
            for (const BufAccess& o : open_)
                if (o.buf == n.buf && n.b < o.e && o.b < n.e && (o.write || n.write)) { conflict = true; break; }
            if (conflict) break;
        }
        for (const ImgAccess& n : iacc) {
            if (conflict) break;
            for (const ImgAccess& o : openImg_)
                if (o.img == n.img && overlap(o.m0, o.mc, n.m0, n.mc) && overlap(o.l0, o.lc, n.l0, n.lc) && (o.write || n.write)) { conflict = true; break; }
        }
        if (conflict) {
            VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            mb.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            mb.srcAccessMask = openWrite_ ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_NONE;   // WAR only needs the execution dependency
            mb.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            mb.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dep.memoryBarrierCount = 1; dep.pMemoryBarriers = &mb;
            core_->cmdBarrier2(cmd_, &dep);
            open_.clear(); openImg_.clear(); openWrite_ = false;
        }
        for (const BufAccess& n : acc) { open_.push_back(n); openWrite_ = openWrite_ || n.write; }
        for (const ImgAccess& n : iacc) { openImg_.push_back(n); openWrite_ = openWrite_ || n.write; }
    }
    // Makes pending compute writes visible outside compute. beginRenderPass: only needed when something was written (graphics never
    // writes buffers here); open reads stay tracked so a later dispatch can still order its writes after them. End of list: always,
    // against everything (a later command list may read or overwrite these buffers).
    void flushBufferAccesses(bool endOfList) {
        if ((open_.empty() && openImg_.empty()) || (!openWrite_ && !endOfList)) return;
        VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        mb.srcAccessMask = openWrite_ ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_NONE;
        if (endOfList) {
            mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        } else {
            mb.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
                              VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                              VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            mb.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |
                               VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT |
                               VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
        }
        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.memoryBarrierCount = 1; dep.pMemoryBarriers = &mb;
        core_->cmdBarrier2(cmd_, &dep);
        open_.clear(); openImg_.clear(); openWrite_ = false;
    }
    void finishDispatch() {
        for (auto& si : stor_) if (si.tex) { dirtyTex_ = true; break; }   // a sampled binding of a written texture must be moved back to its read layout
    }

    std::shared_ptr<Core> core_;
    VkCommandPool pools_[kFrames] = {};
    VkCommandBuffer bufs_[kFrames] = {};
    VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    bool recording_ = false, inPass_ = false, labelDepth_ = false;
    RenderPassDesc pass_;
    std::shared_ptr<Pipeline> pipeline_;
    std::array<std::shared_ptr<Buffer>, kMaxUniformSlots> ubo_;
    std::array<std::size_t, kMaxUniformSlots> uboOff_{}, uboRange_{};
    std::array<std::shared_ptr<Texture>, kMaxTextureSlots> tex_;
    std::array<BoundView, kMaxTextureSlots> bv_{};
    std::array<std::shared_ptr<Sampler>, kMaxTextureSlots> smp_;
    bool dirtyUbo_ = true, dirtyTex_ = true;
    VkDescriptorSet uboSet_ = VK_NULL_HANDLE, texSet_ = VK_NULL_HANDLE;
    // compute state (v3)
    struct StorageImage { std::shared_ptr<Texture> tex; VkImageView view = VK_NULL_HANDLE; std::uint32_t m0 = 0, l0 = 0, lc = 1; };
    struct StorageBuf { std::shared_ptr<Buffer> buf; std::size_t offset = 0, range = 0; };
    std::shared_ptr<ComputePipeline> cpipeline_;
    std::array<StorageImage, kMaxStorageImageSlots> stor_{};
    std::array<StorageBuf, kMaxStorageBufferSlots> sbuf_{};
    static constexpr std::size_t kMaxOpenAccesses = 256;
    std::vector<BufAccess> open_;   // storage buffer accesses since the last compute barrier (see trackAccesses)
    std::vector<ImgAccess> openImg_;   // storage image accesses since the last compute barrier
    bool openWrite_ = false;
    bool dirtyStorImg_ = true, dirtyStorBuf_ = true;
    VkDescriptorSet storImgSet_ = VK_NULL_HANDLE, storBufSet_ = VK_NULL_HANDLE;
    VkDescriptorSet boundGfx_[2] = {}, boundCmp_[4] = {};   // last set bound per bind point (graphics and compute are tracked separately)
    std::vector<std::shared_ptr<void>> keep_;
    std::vector<std::shared_ptr<Texture>> swapTex_;
    Swapchain* swapchain_ = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
class Device final : public rhi::Device {
public:
    explicit Device(std::shared_ptr<Core> core) : core_(std::move(core)) {}
    ~Device() override { if (core_ && core_->device) { vkDeviceWaitIdle(core_->device); core_->collect(true); } }
    const DeviceCapabilities& capabilities() const noexcept override { return core_->caps; }
    std::shared_ptr<rhi::Buffer> createBuffer(const BufferDesc& d, const void* init) override { return std::make_shared<Buffer>(core_, d, init); }
    std::shared_ptr<rhi::Texture> createTexture2D(const TextureDesc& d, const void* init) override { return std::make_shared<Texture>(core_, d, init); }
    std::shared_ptr<rhi::Texture> createTexture(const TextureDesc& d, const TextureSubresourceData* data, std::size_t count) override { return std::make_shared<Texture>(core_, d, data, count); }
    bool supportsFormat(TextureFormat f, TextureUsage u) const noexcept override { return core_->formatUsable(f, u); }
    std::shared_ptr<rhi::Sampler> createSampler(const SamplerDesc& d) override { return std::make_shared<Sampler>(core_, d); }
    std::shared_ptr<rhi::Shader> createShader(const ShaderDesc& d) override { return std::make_shared<Shader>(core_, d); }
    std::shared_ptr<rhi::Pipeline> createPipeline(const std::shared_ptr<rhi::Shader>& s, const PipelineDesc& d) override { return std::make_shared<Pipeline>(core_, s, d); }
    std::shared_ptr<rhi::ComputePipeline> createComputePipeline(const std::shared_ptr<rhi::Shader>& s, const ComputePipelineDesc& d) override { return std::make_shared<ComputePipeline>(core_, s, d); }
    std::unique_ptr<rhi::CommandList> createCommandList() override { return std::make_unique<CommandList>(core_); }
    std::unique_ptr<rhi::Swapchain> createSwapchain(const SwapchainDesc& d) override { return std::make_unique<Swapchain>(core_, d); }

    void beginFrame() override { core_->beginFrame(); }
    void endFrame() override { core_->collect(); }
    void waitIdle() override { vkDeviceWaitIdle(core_->device); core_->collect(); }
    bool supportsGpuTimings() const noexcept override { return core_->timestamps; }
    const std::vector<GpuScopeTiming>& gpuTimings() const noexcept override { return core_->resolved; }
    std::uint64_t gpuTimingsFrame() const noexcept override { return core_->resolvedSerial; }

    void submit(rhi::CommandList& list) override {
        auto* cl = dynamic_cast<CommandList*>(&list);
        if (!cl) throw std::invalid_argument("Vulkan: submit() got a command list from another backend");
        VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        cbi.commandBuffer = cl->handle();
        VkSemaphoreSubmitInfo waits[1]{}, signals[2]{};
        std::uint32_t nw = 0, ns = 0;
        if (Swapchain* sc = cl->swapchain()) {
            VkSemaphore s{};
            if (sc->consumeAcquire(s)) {
                waits[nw++] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, s, 0,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, 0};
            }
            if (sc->consumeRenderFinished(s)) signals[ns++] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, s, 0, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0};
        }
        const std::uint64_t value = ++core_->submitted;
        signals[ns++] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, core_->timeline, value, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0};
        VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        si.waitSemaphoreInfoCount = nw; si.pWaitSemaphoreInfos = waits;
        si.commandBufferInfoCount = 1; si.pCommandBufferInfos = &cbi;
        si.signalSemaphoreInfoCount = ns; si.pSignalSemaphoreInfos = signals;
        VK_CHECK(core_->queueSubmit2(core_->queue, 1, &si, VK_NULL_HANDLE));
        core_->frameValue[core_->frame] = value;
    }

private:
    std::shared_ptr<Core> core_;
};

} // namespace anvil::rhi::vk

namespace anvil::rhi {
std::unique_ptr<Device> createVulkanDevice(const DeviceCreateInfo& info, Backend tier) {
    auto core = std::make_shared<vk::Core>();
    core->init(info, tier);
    return std::make_unique<vk::Device>(std::move(core));
}
} // namespace anvil::rhi
