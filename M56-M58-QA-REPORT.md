# ANVIL Engine M56–M58 QA Report

## Implemented
- Three cascaded sun shadow maps in the OpenGL renderer.
- Practical split distribution, slice-corner fitting, texel snapping and per-cascade shadow-caster culling.
- PCSS blocker search and variable-radius 16-tap PCF using comparison and raw-depth texture views.
- Editor controls for shadow distance and cascade split distribution.
- New portable source-contract test for the CSM/PCSS path.

## Validation performed
- Configured dependency-free test build with `ANVIL_ASSET_TESTS_ONLY=ON`.
- Built all portable test targets successfully.
- CTest: **10/10 passed**.
- ZIP integrity is checked after packaging.

## Not validated
- No `glslangValidator` or `glslc` executable was present, so GLSL compilation was not performed.
- No OpenGL context/GPU execution was available; no FPS, visual comparison, or shadow artifact claim is made.
- The full editor/engine target was not built in this dependency-free test configuration.

## Backend scope / remaining work
- CSM and PCSS are integrated into the OpenGL renderer path only.
- Vulkan/D3D12 RHI still use their existing shadow implementation; matching cascade descriptors, depth-array resources and shader bindings remain future work.
- PCSS quality/performance still needs validation on actual GPUs, especially at high resolutions and with dense foliage.
