#include "anvil/Window.h"
#include "anvil/Core.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace anvil {

static void glDebug(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* msg, const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    ANVIL_LOG("GL %s: %s", type == GL_DEBUG_TYPE_ERROR ? "ERROR" : "msg", msg);
}

Window::Window(int width, int height, const char* title, bool vsync, Api api) : api_(api), vsync_(vsync), w_(width), h_(height) {
    ANVIL_ASSERT(glfwInit(), "glfwInit failed");
    if (api_ == Api::OpenGL) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Vulkan / D3D12: the RHI creates the surface + swapchain
    }
    win_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    ANVIL_ASSERT(win_, "Cannot create window (%s)", api_ == Api::OpenGL ? "OpenGL 4.5 required" : "no-API window");
    if (api_ == Api::OpenGL) {
        glfwMakeContextCurrent(win_);
        glfwSwapInterval(vsync ? 1 : 0);
        ANVIL_ASSERT(gladLoadGL((GLADloadfunc)glfwGetProcAddress), "Cannot load OpenGL");
        ANVIL_LOG("GL %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
#ifndef NDEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(glDebug, nullptr);
#else
        (void)glDebug;
#endif
    }

    glfwSetWindowUserPointer(win_, this);
    glfwSetFramebufferSizeCallback(win_, [](GLFWwindow* w, int fw, int fh) {
        static_cast<Window*>(glfwGetWindowUserPointer(w))->onFramebufferSize(fw, fh);
    });
    glfwGetFramebufferSize(win_, &w_, &h_);
}

Window::~Window() {
    if (win_) glfwDestroyWindow(win_);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(win_); }
void Window::close() { glfwSetWindowShouldClose(win_, GLFW_TRUE); }

void Window::pollEvents() {
    glfwPollEvents();
    glm::dvec2 p;
    glfwGetCursorPos(win_, &p.x, &p.y);
    if (first_) { last_ = p; first_ = false; }
    delta_ = glm::vec2(p - last_);
    last_ = p;
}

void Window::swapBuffers() { if (api_ == Api::OpenGL) glfwSwapBuffers(win_); }
void* Window::nativeWindowHandle() const {
#ifdef _WIN32
    return win_ ? static_cast<void*>(glfwGetWin32Window(win_)) : nullptr;
#else
    return nullptr;
#endif
}
bool Window::keyDown(int k) const { return glfwGetKey(win_, k) == GLFW_PRESS; }

void Window::setCursorCaptured(bool c) {
    glfwSetInputMode(win_, GLFW_CURSOR, c ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    first_ = true;
}

} // namespace anvil
