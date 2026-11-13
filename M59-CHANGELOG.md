# M59 — GPU Occlusion Culling foundation

- Added `shaders/rhi/gpu_occlusion_cull.comp`, a Vulkan GLSL 4.50 compute shader that tests world-space bounding spheres against a previous-frame hierarchical depth pyramid.
- Uses five conservative depth samples and keeps uncertain/off-screen/near-plane cases visible to avoid aggressive false culling.
- Emits one visibility flag per object through an SSBO.
- Added a shader contract test.
- **Integration status:** the shader is not yet dispatched by the renderer; depth-pyramid construction, GPU buffers, barriers and indirect draw consumption remain future integration work.
