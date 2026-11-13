# M50–M52 QA Report

## Changes covered
- M50: 16-tap rotated Poisson PCF in OpenGL and RHI lighting shaders.
- M51: optional split-sum IBL evaluation using irradiance cubemap, prefiltered cubemap, and BRDF LUT. Both variants retain a disabled-by-default gate and analytic ambient fallback.
- M52: height-aware analytic fog transmittance in OpenGL and RHI lighting shaders.
- Extended `PhotorealismPipelineContractTests.cpp` to check the new shader contracts.

## Executed checks
- Configured `ANVIL_ASSET_TESTS_ONLY=ON` in Release mode.
- Built all portable test targets successfully.
- CTest: **8/8 passed**.
- ZIP integrity checked with `unzip -t`.

## Not verified / limitations
- No `glslangValidator` or `glslc` was available, so GLSL/SPIR-V compilation was not performed.
- No OpenGL, Vulkan, D3D11, or D3D12 GPU run, visual capture, or frame-time measurement was performed.
- M50 improves filtering of the existing single shadow map; it does not yet implement CSM rendering or cascade selection.
- M51 adds shader-side IBL evaluation, but the renderer does not yet create and bind the environment resources or dispatch the preprocessing shaders. The gates must remain disabled until runtime resource plumbing is completed.
- M52 is an analytic height-fog approximation, not full volumetric/froxel fog. Rain and wet-surface weather systems are not implemented.
- The full engine build remains unverified in this environment because it requires third-party dependencies beyond the portable test configuration.
