# M45 — D3D12 Compute QA hardening

## Fixed
- Corrected the native readback oracle. The M44 test initialized the UAV buffer to zero but expected values as if it had been initialized with element indices; that made the expected results inconsistent with the actual test input. The test now initializes `values[i] = i` and validates both dispatch ranges against the shader's arithmetic.
- Added explicit fence-wait failure handling. The test now detects `UINT64_MAX` (device removed), checks `SetEventOnCompletion`, validates `WaitForSingleObject`, and reports event-creation errors instead of potentially continuing after a failed wait.
- Added a WARP fallback when no hardware D3D12 adapter is present, so a Windows machine without a discrete/physical adapter can still exercise the D3D12 API path where the runtime supports it.

## Added
- `tests/ComputeShaderContractTests.cpp`: platform-independent contract test for the compute shader's GLSL version, workgroup size, set/binding convention, push constants, bounds check, and CPU reference result for the same direct + indirect dispatch sequence.
- Registered the contract test in both the dependency-free asset-test configuration and the regular `BUILD_TESTING` configuration.

## Validation performed
- Configured and built with `ANVIL_ASSET_TESTS_ONLY=ON`.
- CTest: **6/6 passed**, including the new compute shader contract test.
- ZIP integrity checked after packaging.

## Not run here
- Native D3D12 compilation, DXIL generation, Debug Layer, GPU-Based Validation, and GPU/WARP execution are Windows-only and were not run on this Linux host. The native test is prepared to use hardware first and WARP as fallback on Windows.
- Public backend-neutral compute API integration remains a separate milestone; M45 hardens the existing native validation slice and does not claim `rhi::CommandList` compute support is complete.
