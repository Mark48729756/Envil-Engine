# ANVIL ENGINE — M35: Renderer on the RHI (stage 3 of 5)

Next: stage 4 — Direct3D 12 backend (`createD3D12Device`, DXIL via `cmake/AnvilShaders.cmake` with `D3D12`).

## What is in this ZIP
- `RhiRenderer` (`engine/include/anvil/RhiRenderer.h`, `engine/src/RhiRenderer.cpp`): sun shadow map (hardware PCF) → G-buffer (3 RT + depth) → Cook-Torrance lighting with SSAO / SSR / fog → 5-level bloom → ACES + FXAA → LDR texture → blit to the swapchain. Written only against the RHI; the OpenGL `Renderer` is untouched and still used by `sandbox` / `editor`.
- Shaders in `shaders/rhi/` (Vulkan GLSL 450): `fullscreen.vert gbuffer.{vert,frag} shadow.{vert,frag} lighting.frag bloom_down.frag bloom_up.frag post.frag blit.frag`. CMake target `anvil_rhi_shaders` compiles them to `<build>/shaders/rhi/*.spv` and is a dependency of `anvil`.
- `Mesh` / `Texture2D` keep a CPU copy and create their GL objects lazily on first `draw()` / `id()`. They no longer need a GL context to be created, so they work in a no-API (Vulkan) window. GL behaviour is unchanged.
- `sandbox/vk_sandbox.cpp` (`vk_sandbox` target): the 7×5 PBR sphere grid with 3 orbiting lights on Vulkan.

## Build
`cmake -B build -DANVIL_WITH_VULKAN=ON && cmake --build build --target vk_sandbox`  (`ANVIL_BACKEND=vulkan12` forces the 1.2 tier).

## Usage
```
dev->beginFrame(); auto bb = sc->acquire(); cmd->begin();
renderer.render(*cmd, cam, items, lights, sun, bb);   // bb optional; renderer.presentToScreen = false -> only renderer.outputTexture()
cmd->end(); dev->submit(*cmd); sc->present(); dev->endFrame();
```

## Conventions applied (from the M34 notes)
- GL → Vulkan clip space fix (`z' = 0.5·z + 0.5·w`) is multiplied into the camera and light matrices inside `RhiRenderer`; `Camera` and global GLM settings are not changed.
- Viewport is flipped by the backend; fullscreen shaders output `vUV = (x, 1 − y)`; `lighting.frag` converts uv ↔ ndc with `uvToNdc/ndcToUv` (SSAO, SSR, shadow lookup, depth reconstruction).
- Per-draw data in push constants; all per-frame data (matrices, sun, fog, 32 lights) in one UBO written once per frame, so the "last write wins" rule of dynamic buffers is never hit.
- Textures are bound before `beginRenderPass`, and unused slots are cleared on every pass, so the backend never has to split a pass.
- Swapchain `*_SRGB` formats get `pow(c, 2.2)` in `blit.frag` (post already gamma-encodes); the default `BGRA8` UNORM swapchain is a plain copy.
- Meshes/textures are uploaded on first use, cached by unique id, and dropped after 300 unused frames.
- `outputTexture()` has top-left origin: stage-5 ImGui must use UV (0,0)-(1,1), not the flipped GL UVs.

## Not verified
Per your request nothing was compiled or run (no glslc/Vulkan here). First build may show typos or shader compile errors.
