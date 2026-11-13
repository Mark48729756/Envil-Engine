# ANVIL ENGINE — M34: Vulkan 1.4 backend (stage 2 of 5)

Next: stage 3 — port `Renderer` (shadow → G-buffer → lighting → bloom → tonemap) onto the RHI.

## What is in this ZIP
- `engine/src/rhi/VulkanRHI.cpp` — full backend behind `ANVIL_WITH_VULKAN` (`-DANVIL_WITH_VULKAN=ON`, needs the Vulkan SDK; VMA 3.3.0 is fetched by CMake).
  - `Backend::Vulkan14` = requires a 1.4 instance + device. `Backend::Vulkan12` = 1.2+ device with KHR dynamic rendering / sync2. One code path for both.
  - Instance/device/queue selection (discrete GPU + most VRAM, `preferredAdapter`, `allowSoftwareAdapter`), validation layer + debug messenger.
  - VMA memory, dynamic rendering, sync2 barriers, timeline semaphore for frames in flight (2), deferred destruction of resources.
  - Swapchain (resize/out-of-date/minimised handled, vsync/mailbox, optional HDR10), backbuffer is an RHI `Texture`.
  - Automatic image layout tracking and barriers (renderer never records them); a texture bound inside a pass that is not yet readable splits the pass (resume with Load).
  - Buffers (`dynamic` = per-frame copies, safe to update every frame), textures (init data, mips via blit, `generateMipmaps`, RGB8 expanded to RGBA8), samplers (PCF compare, anisotropy, border), pipelines (all of `PipelineDesc`), push constants, UBO slots 0..3, texture slots 0..15, instanced draws, debug labels.
- `sandbox/vk_triangle.cpp` + `shaders/vk/triangle.{vert,frag}` — smoke test (`vk_triangle` target; compiles shaders with `glslc` through `cmake/AnvilShaders.cmake`).

## Build
`cmake -B build -DANVIL_WITH_VULKAN=ON && cmake --build build --target vk_triangle`  (`ANVIL_BACKEND=vulkan12` forces the 1.2 tier).

## Conventions the Renderer port (stage 3) must follow
- Viewport is flipped (negative height): GL-style clip-space Y and winding work unchanged. Texture origin is top-left, so fullscreen passes must flip V (or use `1 - uv.y`) compared with GL.
- Depth range is 0..1: use `GLM_FORCE_DEPTH_ZERO_TO_ONE` for the Vulkan path.
- Use push constants for per-draw data. `dynamic` buffers hold one value per frame: calling `update()` between draws of the same frame is "last write wins".
- One command list per frame may render to the backbuffer; command lists must be submitted in recording order.
- `StoreOp::DontCare` is treated as Store; MSAA resolve is not implemented yet.
- Static-buffer `update()` and initial uploads go through a blocking staging copy (fine for loading, avoid per-frame use).

## Not verified
Per your request nothing was compiled or run. First build may show typos; the OpenGL path is untouched.
