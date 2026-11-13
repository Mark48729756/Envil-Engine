# ANVIL ENGINE — M33: RHI v2 foundation (Vulkan 1.4 + Direct3D 12, stage 1 of 5)

Roadmap for the two new renderers (each stage is one ZIP):
1. **RHI v2 + no-API window + shader toolchain (this ZIP)**
2. Vulkan 1.4 backend (instance/device/queues, memory, swapchain, dynamic rendering, push descriptors, sync2)
3. Port `Renderer` (shadow → G-buffer → lighting → bloom → tonemap) onto the RHI; Vulkan-GLSL shader set
4. Direct3D 12 backend (device, DXGI flip swapchain, descriptor heaps, root signature, PSOs, fences)
5. Editor: ImGui Vulkan/DX12 backends (viewport shows the RHI output texture), `ANVIL_BACKEND` switch

## What changed
- **`rhi/RHI.h` v2**, fully source-compatible with the OpenGL backend (all new virtuals have defaults):
  `Backend::Vulkan14`, `TextureUsage`, vertex layout, full pipeline state (depth compare, blend modes, cull, topology, formats,
  push-constant size), `RenderPassDesc` (load/store/clear), `Swapchain`, frame loop (`beginFrame / submit / endFrame / waitIdle`),
  `setViewport/Scissor`, `bindUniformBuffer`, `pushConstants`, instanced draws, `generateMipmaps`, debug groups,
  `DeviceCreateInfo` (validation, adapter choice), `parseBackend()`, `backendFromEnvironment()` (`ANVIL_BACKEND=vulkan|dx12|gl`).
- **One binding model for all APIs** (documented in `RHI.h`): UBOs `set 0`, textures `set 1`, push constants for per-draw data.
  Barriers/layout transitions are tracked inside the backend, the renderer never records them.
- **`Window::Api::None`** (`GLFW_NO_API`) + `nativeWindowHandle()` (HWND). `Application(w, h, title, backend)` picks the window type.
- **Shader toolchain** `cmake/AnvilShaders.cmake`: Vulkan-GLSL → `glslc` → SPIR-V → `spirv-cross` → HLSL → `dxc` → DXIL.
  Needs only the LunarG Vulkan SDK. `rhi/ShaderBlob.*` loads the compiled blobs.
- `isBackendAvailable()` now depends on `ANVIL_WITH_VULKAN` / `ANVIL_WITH_D3D12`; both are still **off / not implemented** in this ZIP.

## Verification
Per your request nothing was built or run for this stage. The changes are header/CMake/window plumbing only; the OpenGL path is
untouched (renderer, editor, shaders unchanged). First compile will show any typo.

## Next ZIP (stage 2): Vulkan 1.4
Core 1.4 gives us push descriptors, dynamic rendering, sync2, maintenance5/6 and `vkCmdPushDescriptorSet` without extensions;
a 1.2 fallback tier (`Backend::Vulkan12`) uses the equivalent KHR extensions.
