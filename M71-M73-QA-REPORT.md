# ANVIL Engine M71–M73 QA report

## Delivered
- M71: deterministic procedural cloud field and cloud-shadow CPU reference; Vulkan GLSL volumetric integration prototype.
- M72: SH9 diffuse irradiance evaluation and weighted probe sampling; Vulkan GLSL SH lighting prototype.
- M73: skeleton validation, animation keyframe sampling, quaternion slerp, clip playback, two-bone planar IK; Vulkan GLSL four-weight skinning prototype.

## Validation
- Portable CMake configuration uses `ANVIL_ASSET_TESTS_ONLY=ON` and Debug mode so `assert` checks remain active.
- New CPU tests cover cloud bounds/density/shadow, SH evaluation/probe validity, animation sampling/IK/skeleton validation.
- Shader contract test checks required cloud, SH and skinning interfaces as source text only.

## Not verified / integration remaining
- GLSL was not compiled to SPIR-V unless `glslangValidator`/`glslc` is available and explicitly run.
- No GPU execution, screenshots, visual-quality or FPS tests were performed.
- Cloud noise texture generation, temporal reprojection, cloud shadow injection and renderer pass scheduling are not wired.
- GI probe capture/update, spatial acceleration, specular GI and renderer bindings are not wired; SH9 provides diffuse probe lighting, not ray tracing.
- glTF animation import, hierarchy global-pose evaluation, animation graphs, root motion and GPU bone-palette upload are not wired.
