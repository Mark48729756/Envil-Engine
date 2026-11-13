# M46 QA report

## Completed checks

Command:

```sh
cmake -S . -B build-qa -DANVIL_ASSET_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-qa -j2
ctest --test-dir build-qa --output-on-failure
```

Result: **7/7 passed**

- Asset Database
- RHI Selection
- UI Layout
- GPU Profiler
- SPIR-V Reflection
- Compute Shader Contract
- Rendering Shader Contract (GL/RHI feature parity)

## Build limitation

A full engine configure was attempted, but CMake FetchContent could not clone GLFW because `github.com` DNS/network access was unavailable. Consequently the complete engine C++ build and the GLM-based frustum unit-test binary were not run here. No GLSL compiler was present, and no real GPU runtime validation was possible.

## Runtime verification still required

On a workstation with dependencies and a graphics device:

1. Configure and build the full engine.
2. Run the full CTest suite including `anvil_frustum_culling_tests`.
3. Run the OpenGL sandbox/editor and verify that camera-cull counts increase when moving objects outside the view.
4. Verify shadow-only casters still contribute inside the light frustum.
5. Run the Vulkan/RHI path and compare visible geometry/shadows against OpenGL.
6. Compile all GL/Vulkan/DXIL shader variants with the project's shader toolchain.
