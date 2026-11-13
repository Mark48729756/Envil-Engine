// Vulkan backend smoke test: vertex buffer + dynamic UBO + push constants + swapchain resize.
// Run with ANVIL_BACKEND=vulkan12 to force the 1.2 tier.
#include "anvil/Window.h"
#include "anvil/rhi/RHI.h"
#include "anvil/rhi/ShaderBlob.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <exception>

int main() try {
    using namespace anvil; using namespace anvil::rhi;
    Window win(1280, 720, "ANVIL Vulkan triangle", true, Window::Api::None);
    Backend backend = backendFromEnvironment(Backend::Vulkan14);
    if (backend == Backend::OpenGL45) backend = Backend::Vulkan14;
    DeviceCreateInfo ci;
#ifndef NDEBUG
    ci.validation = true;
#endif
    ci.nativeWindow = win.native();
    auto dev = createDevice(backend, ci);
    auto sc = dev->createSwapchain({win.native(), win.width(), win.height(), true, 3, false});

    struct V { float x, y, r, g, b; };
    const V verts[3] = {{0.0f, 0.6f, 1, 0, 0}, {0.6f, -0.5f, 0, 1, 0}, {-0.6f, -0.5f, 0, 0, 1}};
    BufferDesc vb; vb.size = sizeof(verts); vb.usage = BufferUsage::Vertex; vb.label = "triangle";
    auto vbuf = dev->createBuffer(vb, verts);
    BufferDesc ub; ub.size = 16; ub.usage = BufferUsage::Uniform; ub.dynamic = true; ub.label = "tint";
    auto ubuf = dev->createBuffer(ub);

    PipelineDesc pd;
    pd.depthTest = false; pd.depthWrite = false; pd.cull = false;
    pd.vertexLayout.bindings = {{0, sizeof(V), false}};
    pd.vertexLayout.attributes = {{0, 0, 0, VertexFormat::Float2}, {1, 0, 8, VertexFormat::Float3}};
    pd.colorFormats = {sc->format()}; pd.pushConstantBytes = 4; pd.label = "triangle";
    auto shader = dev->createShader(loadShaderDesc(backend, ANVIL_VK_DEMO_SHADER_DIR, "triangle.vert", "triangle.frag"));
    auto pipe = dev->createPipeline(shader, pd);
    auto cmd = dev->createCommandList();

    float angle = 0.0f;
    while (!win.shouldClose()) {
        win.pollEvents();
        if (win.resized()) sc->resize(win.width(), win.height());
        dev->beginFrame();
        auto bb = sc->acquire();
        if (!bb) { dev->endFrame(); continue; }
        angle += 0.01f;
        const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        ubuf->update(0, tint, sizeof(tint));
        cmd->begin();
        RenderPassDesc rp; rp.label = "main";
        ColorAttachment ca; ca.texture = bb; ca.clear[0] = 0.05f; ca.clear[1] = 0.06f; ca.clear[2] = 0.09f; ca.clear[3] = 1.0f;
        rp.colors.push_back(ca);
        cmd->beginRenderPass(rp);
        cmd->setPipeline(pipe);
        cmd->bindUniformBuffer(0, ubuf);
        cmd->pushConstants(&angle, 4);
        cmd->bindVertexBuffer(vbuf, 0);
        cmd->draw(3);
        cmd->endRenderPass();
        cmd->end();
        dev->submit(*cmd);
        sc->present();
        dev->endFrame();
    }
    dev->waitIdle();
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "vk_triangle: %s\n", e.what()); return 1; }
