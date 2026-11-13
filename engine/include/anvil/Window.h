#pragma once
#include <glm/glm.hpp>

struct GLFWwindow;

namespace anvil {

class Window {
public:
    // OpenGL: creates a GL 4.5 core context (default, unchanged behaviour).
    // None:   no client API (GLFW_NO_API) - used by the Vulkan and Direct3D 12 backends, which own their swapchain.
    enum class Api { OpenGL, None };

    Window(int width, int height, const char* title, bool vsync = true, Api api = Api::OpenGL);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* native() const { return win_; }
    Api api() const { return api_; }
    // Win32 HWND (Windows only, nullptr elsewhere) for the D3D12 swapchain. Vulkan creates its surface from native().
    void* nativeWindowHandle() const;
    bool shouldClose() const;
    void close();
    void pollEvents();
    void swapBuffers();            // no-op for Api::None (the RHI swapchain presents)
    bool vsync() const { return vsync_; }

    int width() const { return w_; }
    int height() const { return h_; }
    bool resized() { bool r = resized_; resized_ = false; return r; }

    bool keyDown(int glfwKey) const;
    glm::vec2 mouseDelta() const { return delta_; }
    void setCursorCaptured(bool captured);

    // internal (GLFW callback)
    void onFramebufferSize(int w, int h) { w_ = w; h_ = h; resized_ = true; }

private:
    GLFWwindow* win_ = nullptr;
    Api api_ = Api::OpenGL;
    bool vsync_ = true;
    int w_ = 0, h_ = 0;
    bool resized_ = false;
    glm::dvec2 last_{0.0};
    glm::vec2 delta_{0.0f};
    bool first_ = true;
};

} // namespace anvil
