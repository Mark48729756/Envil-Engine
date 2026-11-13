# M68 — Water rendering foundations

- Added `WaterSurface.h`: deterministic CPU reference for layered directional waves, analytic surface normals, Schlick Fresnel and shoreline foam weighting.
- Added `shaders/rhi/water.frag`: Vulkan GLSL water composition prototype with scene-color refraction, cubemap reflection, depth-derived foam and configurable tint.
- Renderer-side water mesh submission, scene-color/depth copy scheduling, descriptor binding and actual GPU validation remain pending.
