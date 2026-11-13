# ANVIL RHI (initial implementation)

The RHI is the backend boundary, not a second renderer. Application-facing code should depend on `RHI.h`; backend-specific implementation details belong in `OpenGLRHI.h`.

Current OpenGL 4.5 coverage: buffer allocation/update, 2D texture allocation/upload, sampler creation, vertex+fragment shader compile/link, basic fixed-state pipeline, immediate draw/draw-indexed commands, texture/sampler binding, and device capability query.

Not implemented yet: Vulkan/D3D12 backends, swapchain abstraction, render passes/frame graph, descriptor sets, compute, indirect draws, synchronization primitives, async upload queues, shader reflection, pipeline cache, and automatic vertex input layout. These are deliberately separate follow-up tasks rather than pretending the initial RHI is feature-complete.

`ResourceManager` is an explicit shared-ownership registry with ID lookup, release, snapshots and byte/count statistics. It does not infer GPU allocation sizes: callers supply byte estimates when retaining resources. Destroy resources while the owning graphics context is current.

## Backend selection status (M20–M25)
`Backend` now names OpenGL45, Vulkan12 and Direct3D12. `isBackendAvailable()` returns true only for OpenGL45 in this revision. `createDevice()` throws a clear error for Vulkan/D3D12 rather than pretending a placeholder is a working device. The native Vulkan and D3D12 implementations still need resource objects, shader/pipeline compilation, command recording, synchronization and swapchain/presentation.

## Vulkan (M34)
Enable with `-DANVIL_WITH_VULKAN=ON`. See `M34-CHANGELOG.md` for conventions (flipped viewport, depth 0..1, push constants).

## RHI v2 (M33)
Adds the explicit-API surface used by the Vulkan 1.4 and Direct3D 12 backends: `Swapchain`, `RenderPassDesc`, full `PipelineDesc`, `VertexLayout`, push constants, `bindUniformBuffer`, instanced draws and `beginFrame/submit/endFrame`. The OpenGL backend keeps throwing defaults for these (its renderer still talks to GL directly). Binding model: UBO `set 0`, textures `set 1`, push constants for per-draw data. Compile shaders with `cmake/AnvilShaders.cmake` and load them with `rhi/ShaderBlob.h`.

## Renderer on the RHI (M35)
`RhiRenderer` runs the whole deferred pipeline through this interface (Vulkan today, D3D12 in stage 4). Shaders: `shaders/rhi/`. See `M35-CHANGELOG.md`.

## Compute on OpenGL (M43 step 7)
OpenGL 4.3+ supports compute (`ShaderDesc::computeSource`, dispatch, storage buffers / 2D storage images). Write compute shaders in GL GLSL with flat binding points (see `OpenGLRHI.h`); push constants and array / cube / 3D storage images are not available on GL. D3D12 compute is not implemented.
