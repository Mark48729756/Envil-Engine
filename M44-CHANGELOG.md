# M44 — D3D12 Compute validation slice

## Added
- `shaders/rhi/compute_validation.comp`: first real compute shader, authored in the engine GLSL dialect (64 threads/group, storage-buffer UAV, two push constants).
- DXIL build path uses the existing `glslc -> SPIR-V -> spirv-cross HLSL -> dxc` pipeline.
- `tests/d3d12/D3D12ComputeValidation.cpp`: Windows-only native validation executable. It creates a UAV root-signature descriptor table, a compute PSO, records direct `Dispatch`, inserts a UAV barrier, records `ExecuteIndirect` using a DISPATCH command signature, inserts a second UAV barrier, transitions/copies to readback, and validates all 128 values on CPU.
- Debug Layer and GPU-Based Validation are enabled before device creation when the installed Windows SDK/runtime exposes them. D3D12 InfoQueue errors/corruption are treated as test failures.

## Build / run (Windows + Vulkan SDK)
```powershell
cmake -S . -B build -DANVIL_WITH_D3D12=ON -DBUILD_TESTING=ON
cmake --build build --config Release --target anvil_d3d12_compute_validation
ctest --test-dir build -C Release -R anvil_d3d12_compute_validation --output-on-failure
```

The test requires a hardware D3D12 adapter. Exit code 77 means skipped because no hardware adapter was found.

## Important implementation boundary
This milestone adds a real, executable D3D12 compute validation path and the shader compilation target. It does **not yet wire compute into the public backend-neutral `rhi::CommandList` methods** (`setComputePipeline`, `bindStorageBuffer`, `dispatch`, `dispatchIndirect`): the current D3D12 backend still rejects storage/indirect buffers and storage textures. That API integration requires replacing the existing upload-only buffer allocation model with default-heap UAV-capable resources, adding per-frame descriptor-table allocation for UAVs, and adding access-range hazard tracking. The test is deliberately standalone so it validates native D3D12 mechanics without falsely claiming the engine RHI is already integrated.

## Validation performed in this environment
- ZIP structure and source integration reviewed.
- Native D3D12 compile/runtime validation was **not run** here (this environment is not a Windows D3D12 host and has no GPU validation runtime).
