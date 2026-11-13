#include "anvil/rhi/RHI.h"
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace anvil::rhi {
namespace {
#ifdef ANVIL_WITH_VULKAN
constexpr bool kHasVulkan = true;
#else
constexpr bool kHasVulkan = false;
#endif
#if defined(ANVIL_WITH_D3D12) && defined(_WIN32)
constexpr bool kHasD3D12 = true;
#else
constexpr bool kHasD3D12 = false;
#endif
std::string lower(const char* s) { std::string r; for (; s && *s; ++s) r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*s)))); return r; }
}

const char* backendName(Backend backend) noexcept {
    switch (backend) {
        case Backend::OpenGL45: return "OpenGL 4.5";
        case Backend::Vulkan12: return "Vulkan 1.2";
        case Backend::Vulkan14: return "Vulkan 1.4";
        case Backend::Direct3D12: return "Direct3D 12";
    }
    return "Unknown";
}
bool isBackendAvailable(Backend backend) noexcept {
    switch (backend) {
        case Backend::OpenGL45: return true;
        case Backend::Vulkan12:
        case Backend::Vulkan14: return kHasVulkan;
        case Backend::Direct3D12: return kHasD3D12;
    }
    return false;
}
std::unique_ptr<Device> createDevice(Backend backend, const DeviceCreateInfo& info) {
    if (!isBackendAvailable(backend))
        throw std::runtime_error(std::string(backendName(backend)) +
            " backend is not implemented in this build; select OpenGL 4.5 instead");
    switch (backend) {
        case Backend::OpenGL45: (void)info; return createOpenGL45Device();
#ifdef ANVIL_WITH_VULKAN
        case Backend::Vulkan12:
        case Backend::Vulkan14: return createVulkanDevice(info, backend);
#endif
#if defined(ANVIL_WITH_D3D12) && defined(_WIN32)
        case Backend::Direct3D12: return createD3D12Device(info);
#endif
        default: break;
    }
    throw std::runtime_error(std::string(backendName(backend)) + " backend is not compiled into this build");
}
bool parseBackend(const char* text, Backend& out) noexcept {
    if (!text) return false;
    const std::string s = lower(text);
    if (s == "gl" || s == "opengl" || s == "opengl45") { out = Backend::OpenGL45; return true; }
    if (s == "vulkan" || s == "vk" || s == "vulkan14" || s == "vulkan1.4") { out = Backend::Vulkan14; return true; }
    if (s == "vulkan12" || s == "vulkan1.2") { out = Backend::Vulkan12; return true; }
    if (s == "d3d12" || s == "dx12" || s == "directx12" || s == "direct3d12") { out = Backend::Direct3D12; return true; }
    return false;
}
Backend backendFromEnvironment(Backend fallback) noexcept {
    Backend b{};
    if (parseBackend(std::getenv("ANVIL_BACKEND"), b) && isBackendAvailable(b)) return b;
    return fallback;
}
} // namespace anvil::rhi
