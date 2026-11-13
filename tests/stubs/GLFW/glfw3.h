
#pragma once
#define GLFW_JOYSTICK_1 0
#define GLFW_JOYSTICK_LAST 15
#define GLFW_PRESS 1
#define GLFW_TRUE 1
#define GLFW_GAMEPAD_BUTTON_LAST 14
#define GLFW_GAMEPAD_AXIS_LAST 5
#define GLFW_HAT_UP 1
#define GLFW_HAT_RIGHT 2
#define GLFW_HAT_DOWN 4
#define GLFW_HAT_LEFT 8
struct GLFWgamepadstate { unsigned char buttons[15]; float axes[6]; };
extern "C" { int glfwJoystickPresent(int); const char* glfwGetJoystickName(int); const char* glfwGetJoystickGUID(int); int glfwJoystickIsGamepad(int); const unsigned char* glfwGetJoystickButtons(int,int*); const float* glfwGetJoystickAxes(int,int*); const unsigned char* glfwGetJoystickHats(int,int*); int glfwGetGamepadState(int,GLFWgamepadstate*); int glfwUpdateGamepadMappings(const char*); int glfwSetJoystickRumble(int,float,float); }
