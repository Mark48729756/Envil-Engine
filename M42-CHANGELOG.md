# ANVIL ENGINE — M42: GPU profiler (timestamp queries) — Sprint 1, module 1

## Added
- `rhi::GpuScopeTiming`, `Device::supportsGpuTimings()`, `Device::gpuTimings()`, `Device::gpuTimingsFrame()` (RHI.h, v3 section).
  Defaults report "unsupported" so OpenGL and D3D12 backends compile unchanged.
- Vulkan: one timestamp query pool per frame slot, fixed-size scope tables (zero allocations while recording or resolving).
  Every `pushDebugGroup/popDebugGroup` pair and every labelled render pass is one timed scope, so all existing passes
  (Shadow, G-buffer, Lighting, Bloom down/up, Tonemap, Present) are measured with no change to `RhiRenderer.cpp`.
  Render-pass scopes now bracket the whole pass (layout transitions + clears before, store ops after).
- Read-back happens in `beginFrame()` after the frame-slot timeline wait: no stalls, latency = framesInFlight (2 frames).
- `anvil::GpuProfiler` (GpuProfiler.h/.cpp): EMA + decaying peak per scope, frame total (depth-0 scopes), 16.6 ms budget flag,
  plain-text report. UI-agnostic; the editor overlay is the next module.
- `tests/GpuProfilerTests.cpp` (dependency-free, registered in both CMake test blocks).

## Rules / limits
- The first CommandList begun in a frame resets that frame's query pool: submit it first if you record several lists per frame.
- 96 scopes per frame, nesting depth 32; extra scopes are skipped, push/pop stay balanced.
- Timings are GPU wall time of each scope (ALL_COMMANDS timestamps); overlapping work is not separated.

## Verification
- `GpuProfilerTests` built with `-Wall -Wextra -Werror` (g++, C++20) and passed; RHI.h syntax-checked.
- `VulkanRHI.cpp` was NOT compiled here (no Vulkan SDK in this environment). Build it with `-DANVIL_WITH_VULKAN=ON` and run with
  validation (`DeviceCreateInfo::validation = true`) before relying on it.
