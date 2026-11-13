# ANVIL Engine M47-M49 — Photorealism Foundations

This delivery advances three milestones together. It adds substantial renderer foundations, but it is not a claim of GTA 6 / Microsoft Flight Simulator parity.

## M47 — Physically based atmosphere and volumetric clouds
- Replaced the flat gradient sky in both OpenGL and RHI lighting shaders with view-ray integration using exponential Rayleigh and Mie density profiles, phase functions, and view/sun optical-depth approximations.
- Added a genuinely volumetric 3D cloud field: 3D value-noise/fBm density, altitude envelope, view-ray marching, sun-light marching, transmittance and early termination.
- Added `RenderSettings::cloudTime` for OpenGL cloud advection. The current RHI frame layout has no cloud-time field yet, so its cloud field is static.
- Bounded the first implementation to 12 atmosphere steps, 16 cloud steps and two cloud-light samples per cloud sample. This is a performance-conscious starting point, not a final half-resolution/temporal cloud solution.

## M48 — Image-based lighting shader pipeline
- Added equirectangular HDR-to-cubemap conversion compute shader.
- Added diffuse irradiance convolution compute shader.
- Added GGX specular environment prefilter compute shader for roughness mip levels.
- Added split-sum BRDF integration LUT compute shader.
- These are pipeline stages and shader assets; environment texture allocation, mip dispatch, synchronization, caching and runtime binding are not yet wired into either renderer.

## M49 — PBR material map support
- Extended `Material` with normal, roughness, metallic and AO texture references.
- OpenGL G-buffer now supports tangent-space normal mapping using derivative-built tangent frames, plus roughness/metallic/AO maps.
- Material AO is carried through the material G-buffer channel and modulates ambient occlusion in lighting.
- Existing materials remain compatible when the optional maps are null.
- Current RHI G-buffer still uses the previous single-albedo-map binding contract. Full material-map parity requires expanding its descriptor/bindless material system and is not claimed here.

## Known limitations
- The cloud field is procedural 3D density, not a weather-driven cloud simulation or artist-authored volumetric asset system.
- No GPU timing, visual capture, or driver validation was available in this environment.
- IBL compute shaders are not yet dispatched by the runtime renderer.
- OpenGL material maps are wired; RHI material maps are pending.
- The existing PPM loader is still the only built-in texture file decoder.
- Shader source contract tests do not replace GLSL/SPIR-V compiler validation.
