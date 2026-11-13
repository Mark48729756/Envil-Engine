# ANVIL ENGINE — M37: native Direct3D 12 backend (initial integration)

## Added
- Added the Windows-only `ANVIL_WITH_D3D12` CMake option and native D3D12 RHI source.
- Device/adapter selection (hardware preference, optional WARP), command queue/fence frame pacing, root signature, descriptor heaps and GLFW HWND swapchain.
- Upload-heap buffers, 2D textures with SRV/RTV/DSV views, samplers, DXIL shader loading, graphics PSOs, render-pass attachments, resource transitions, descriptor binding, push constants and indexed/instanced draw calls.
- Enabling `ANVIL_WITH_D3D12` automatically enables the existing GLSL -> SPIR-V -> HLSL -> DXIL shader build pipeline.

## Configure on Windows
```powershell
cmake -S . -B build -DANVIL_WITH_D3D12=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requirements: Windows 10/11 SDK, MSVC, GLFW's Win32 native access, Vulkan SDK tools (`glslc`, `spirv-cross`, `dxc`) and a D3D12-capable GPU with Shader Model 6.0.

## Verification limits
- The implementation was authored in a Linux environment without Windows SDK, DXC or a D3D12 device. It has **not** been compiled with MSVC or run on Windows.
- The existing `RhiRenderer` is still a caller-owned RHI renderer; the current editor/sandbox entry points remain on the legacy OpenGL renderer. This milestone does not silently switch the editor's rendering path.
- Runtime mipmap generation, storage/UAV textures, and ImGui D3D12 texture-handle integration are not implemented in this initial backend. `generateMipmaps()` reports this explicitly.
- Treat this as an initial backend integration requiring Windows compile/runtime validation, not a certified production renderer.
