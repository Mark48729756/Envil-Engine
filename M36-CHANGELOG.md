# ANVIL ENGINE — M36: shared shadow frustum + Direct3D 12 preparation

## Completed in this revision
- Extracted the directional-light shadow camera into `anvil/ShadowMath.h`.
- OpenGL and RHI now call the same texel-snapped frustum builder. The only intentional difference is the API clip-depth convention: OpenGL keeps `[-1, 1]`, Vulkan/D3D-style RHI uses `[0, 1]`.
- Hardened the shared calculation for zero-length sun directions and non-positive radius/map sizes.
- Added opt-in `ANVIL_COMPILE_D3D12_SHADERS` CMake wiring to generate DXIL from the same RHI GLSL sources (requires `glslc`, `spirv-cross`, and `dxc`).
- Kept backend selection conservative: Direct3D 12 is still reported unavailable until a real `D3D12RHI.cpp` implementation exists. The selector must not advertise a backend that cannot create a usable device.

## Direct3D 12 stage status
The project already has a GLSL -> SPIR-V -> HLSL -> DXIL shader pipeline in `cmake/AnvilShaders.cmake`, but it is not yet wired to an actual D3D12 device implementation. This revision does not claim that DX12 rendering works. A correct stage 4 still needs the D3D12 device, descriptor heaps, resource/state tracking, graphics PSOs/root signatures, command submission/fences, and GLFW swapchain implementation, followed by a Windows SDK build and runtime validation.

## Verification
- `cmake -DANVIL_ASSET_TESTS_ONLY=ON -DBUILD_TESTING=ON`: configured successfully.
- Build: passed.
- CTest: 2/2 passed (`anvil_asset_database_tests`, `anvil_rhi_selection_tests`).
- The full renderer and shader toolchain were not built here; this Linux environment does not establish Windows/D3D12 compatibility.
