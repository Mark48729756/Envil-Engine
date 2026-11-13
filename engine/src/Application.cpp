#include "anvil/Application.h"
#include <GLFW/glfw3.h>

namespace anvil {

Application::Application(int w, int h, const std::string& title, rhi::Backend backend)
    : window_(w, h, title.c_str(), true, backend == rhi::Backend::OpenGL45 ? Window::Api::OpenGL : Window::Api::None),
      backend_(backend) {}
Application::~Application() = default;

void Application::run() {
    onInit();
    const float fixedDt = 1.0f / 60.0f;
    double prev = glfwGetTime();
    float acc = 0.0f;
    while (!window_.shouldClose()) {
        double now = glfwGetTime();
        float dt = (float)(now - prev);
        prev = now;
        if (dt > 0.25f) dt = 0.25f;

        window_.pollEvents();
        if (window_.resized()) onResize(window_.width(), window_.height());

        acc += dt;
        while (acc >= fixedDt) { onFixedUpdate(fixedDt); acc -= fixedDt; }
        onUpdate(dt);
        onRender();
        window_.swapBuffers();
    }
}

} // namespace anvil
