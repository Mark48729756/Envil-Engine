#pragma once
#include "Window.h"
#include "rhi/RHI.h"
#include <string>

namespace anvil {

class Application {
public:
    // `backend` picks the window type: OpenGL45 -> GL context, Vulkan/D3D12 -> no-API window (RHI swapchain).
    Application(int width, int height, const std::string& title, rhi::Backend backend = rhi::Backend::OpenGL45);
    virtual ~Application();
    void run();

protected:
    virtual void onInit() {}
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onFixedUpdate(float dt) { (void)dt; }
    virtual void onRender() {}
    virtual void onResize(int w, int h) { (void)w; (void)h; }

    Window window_;
    rhi::Backend backend_ = rhi::Backend::OpenGL45;
};

} // namespace anvil
