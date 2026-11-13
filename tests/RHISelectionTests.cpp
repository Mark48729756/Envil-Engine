#include "anvil/rhi/RHI.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace anvil::rhi {
std::unique_ptr<Device> createOpenGL45Device() { return {}; }
}

int main() {
    using namespace anvil::rhi;
    assert(isBackendAvailable(Backend::OpenGL45));
    assert(!isBackendAvailable(Backend::Vulkan12));
    assert(!isBackendAvailable(Backend::Direct3D12));
    assert(!isBackendAvailable(Backend::Vulkan14));
    assert(std::string(backendName(Backend::Vulkan14)) == "Vulkan 1.4");
    Backend parsed{}; assert(parseBackend("DX12", parsed) && parsed == Backend::Direct3D12);
    assert(parseBackend("vk", parsed) && parsed == Backend::Vulkan14);
    assert(std::string(backendName(Backend::Vulkan12)) == "Vulkan 1.2");
    assert(std::string(backendName(Backend::Direct3D12)) == "Direct3D 12");
    bool vulkanRejected = false, d3dRejected = false;
    try { (void)createDevice(Backend::Vulkan12); } catch (const std::runtime_error&) { vulkanRejected = true; }
    try { (void)createDevice(Backend::Direct3D12); } catch (const std::runtime_error&) { d3dRejected = true; }
    assert(vulkanRejected && d3dRejected);
    std::cout << "RHI selection tests passed\n";
}
