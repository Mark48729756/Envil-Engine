# M117 build fixes (Linux, GCC 13, GLFW 3.4)
- engine/src/Controller.cpp: glfwSetJoystickRumble does not exist in GLFW 3.4 -> plain GLFW joysticks return false (native backends unchanged).
- editor/main.cpp: ProxyBox/WCell were declared after the member functions that use them -> moved above the open-world section.
Result: full build (engine, sandbox, editor) OK, ctest 21/21 + dependency-free suite 53/53 (incl. GltfImport).
- engine/src/Controller.cpp: <Xinput.h> -> <xinput.h> (MinGW/case-sensitive FS).
- engine/src/Renderer.cpp: shader dir now resolved at runtime (env ANVIL_SHADER_DIR, exe dir/shaders, ./shaders) instead of a build-time absolute path.
