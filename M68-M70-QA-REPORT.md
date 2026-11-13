# ANVIL Engine M68–M70 QA report

## Delivered
- M68: CPU wave evaluation, analytic normals, Schlick Fresnel and shoreline foam reference; Vulkan GLSL water composition prototype for refraction/reflection/depth foam.
- M69: CPU parallax traversal reference, normalized material layer blending and triplanar weights; reusable Vulkan GLSL POM/triplanar helpers.
- M70: weather transition controller with rain wetness and snow cover accumulation/melt; Vulkan GLSL precipitation particle compute prototype.

## Automated validation
- Configure: `cmake -S . -B build-qa -DANVIL_ASSET_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.
- Build: successful.
- CTest: **20/20 passed** in Debug (assertions active), including water, material, weather and shader source-contract tests.
- ZIP integrity: checked with `unzip -t` after packaging.

## Not verified / integration remaining
- No `glslangValidator` or `glslc` was available; the new GLSL files were not compiled to SPIR-V.
- No GPU execution, screenshot, visual-quality or FPS test was performed.
- Water shader is a prototype; water geometry, scene color/depth copy, descriptors, render scheduling and RHI/OpenGL/D3D12 parity are not wired into the renderer.
- Material helpers are not yet connected to the runtime material system, texture arrays, tangent-space mesh validation or editor UI.
- Weather compute is not yet dispatched; particle buffers/draw pass and weather-to-material bindings are not integrated.
