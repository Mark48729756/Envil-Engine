// M35 demo: the full deferred renderer (shadow -> G-buffer -> lighting -> bloom -> tonemap) running on the Vulkan RHI.
// WASD + mouse, Shift = fast, E/Q = up/down, Esc = quit.  ANVIL_BACKEND=vulkan12 forces the 1.2 tier.
#include "anvil/Window.h"
#include "anvil/RhiRenderer.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>

int main() try {
    using namespace anvil;
    Window win(1600, 900, "ANVIL Engine - Vulkan sandbox", true, Window::Api::None);
    rhi::Backend backend = rhi::backendFromEnvironment(rhi::Backend::Vulkan14);
    if (backend == rhi::Backend::OpenGL45) backend = rhi::Backend::Vulkan14;
    rhi::DeviceCreateInfo ci;
#ifndef NDEBUG
    ci.validation = true;
#endif
    ci.nativeWindow = win.native();
    auto dev = rhi::createDevice(backend, ci);
    auto sc = dev->createSwapchain({win.native(), win.width(), win.height(), true, 3, false});
    auto cmd = dev->createCommandList();
    RhiRenderer renderer(*dev, win.width(), win.height(), sc->format());

    auto sphere = Mesh::sphere(64);
    auto floorMesh = Mesh::plane(40.0f, 40.0f);
    std::vector<DrawItem> items;
    items.push_back({floorMesh.get(), glm::mat4(1.0f), {{0.55f, 0.55f, 0.58f}, 0.8f, 0.0f}});
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 7; ++x) {
            Material m;
            m.albedo = (y % 2) ? glm::vec3(0.95f, 0.64f, 0.54f) : glm::vec3(0.8f, 0.1f, 0.1f);
            m.roughness = 0.05f + 0.15f * x;
            m.metallic = y / 4.0f;
            items.push_back({sphere.get(), glm::translate(glm::mat4(1.0f), glm::vec3((x - 3) * 2.2f, 1.15f + y * 2.2f, 0.0f)), m});
        }

    Camera cam; cam.position = {0.0f, 3.0f, 14.0f}; cam.pitch = -8.0f;
    Sun sun;
    std::vector<PointLight> lights;
    win.setCursorCaptured(true);
    double prev = glfwGetTime(), t = 0.0;
    while (!win.shouldClose()) {
        win.pollEvents();
        const double now = glfwGetTime(); const float dt = (float)std::min(now - prev, 0.25); prev = now; t += dt;
        if (win.keyDown(GLFW_KEY_ESCAPE)) win.close();
        if (std::getenv("ANVIL_AUTOEXIT") && t > 1.5) win.close();
        if (win.resized() && win.width() > 0 && win.height() > 0) { sc->resize(win.width(), win.height()); renderer.resize(win.width(), win.height()); }

        const glm::vec2 d = win.mouseDelta();
        cam.yaw += d.x * 0.1f;
        cam.pitch = glm::clamp(cam.pitch - d.y * 0.1f, -89.0f, 89.0f);
        const float speed = (win.keyDown(GLFW_KEY_LEFT_SHIFT) ? 20.0f : 7.0f) * dt;
        if (win.keyDown(GLFW_KEY_W)) cam.position += cam.forward() * speed;
        if (win.keyDown(GLFW_KEY_S)) cam.position -= cam.forward() * speed;
        if (win.keyDown(GLFW_KEY_D)) cam.position += cam.right() * speed;
        if (win.keyDown(GLFW_KEY_A)) cam.position -= cam.right() * speed;
        if (win.keyDown(GLFW_KEY_E)) cam.position.y += speed;
        if (win.keyDown(GLFW_KEY_Q)) cam.position.y -= speed;

        lights.clear();
        for (int i = 0; i < 3; ++i) {
            const float a = (float)t * 0.6f + i * 2.094f;
            PointLight l;
            l.position = {std::cos(a) * 9.0f, 4.0f + i * 2.0f, std::sin(a) * 4.0f + 3.0f};
            l.color = i == 0 ? glm::vec3(1.0f, 0.4f, 0.3f) : i == 1 ? glm::vec3(0.3f, 1.0f, 0.5f) : glm::vec3(0.4f, 0.5f, 1.0f);
            l.intensity = 120.0f;
            lights.push_back(l);
        }

        dev->beginFrame();
        auto bb = sc->acquire();
        if (!bb) { dev->endFrame(); continue; }
        cmd->begin();
        renderer.render(*cmd, cam, items, lights, sun, bb);
        cmd->end();
        dev->submit(*cmd);
        sc->present();
        dev->endFrame();
    }
    dev->waitIdle();
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "vk_sandbox: %s\n", e.what()); return 1; }
