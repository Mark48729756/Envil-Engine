# ANVIL ENGINE M37 — D3D12 Fix Report

This patch modifies `engine/src/rhi/D3D12RHI.cpp` only. It is a source-level defensive fix; a Windows/D3D12 build and GPU runtime validation were not available in the current environment.

## Fixed or made fail-fast

- D3D-001: removed the literal `\\n` between the swapchain destructor and `resize()` declaration.
- D3D-002 / D3D-003: descriptor allocation now reserves contiguous blocks sized for the complete CBV, SRV, and sampler tables. Bounds checks use the requested block size.
- D3D-004: added RTV and DSV heap capacity checks, including swapchain back buffers.
- D3D-005: multi-mip initial-data uploads are explicitly rejected instead of silently uploading only mip 0. The public RHI texture creation API has no source byte-count or per-mip layout parameter, so a safe multi-mip upload cannot be inferred.
- D3D-006: texture upload validates the source pointer, overflow, and minimum byte count before reading it.
- D3D-007: sampler filter selection now accounts for both minification and magnification filters, including mip filtering.
- D3D-008: ClampToBorder now maps to D3D12 border addressing and preserves the configured black/white border color.
- D3D-009: anisotropic and comparison-anisotropic filter modes are selected when maxAnisotropy is greater than 1.
- D3D-010: alignUp validates its alignment and checks UINT overflow; buffer sizes that cannot be safely aligned are rejected.
- D3D-011: non-zero buffer updates reject a null source pointer.
- D3D-012: Storage and Readback buffer usages are explicitly rejected because those resource paths are not implemented.
- D3D-013: non-dynamic static upload buffers without initial data are zero-initialized.
- D3D-014: multisample SRV/DSV view dimensions are selected correctly; multisample textures are constrained to one mip and cannot use initial-data uploads.
- D3D-015: Storage texture usage is explicitly rejected because UAV binding is not implemented by the current RHI binding model/backend path.
- D3D-016: acquire() avoids indexing an empty swapchain buffer list and validates the returned back-buffer index when the swapchain has a valid size.
- D3D-017: fence waits and Present failures now include GetDeviceRemovedReason() diagnostics for device removal/reset/hang.
- D3D-018: per-instance vertex attributes now set InstanceDataStepRate to 1.

## Remaining limitations

Storage buffers, Readback buffers, Storage/UAV textures, and multi-mip initial texture uploads remain unsupported and now fail explicitly rather than silently behaving incorrectly. The RHI API currently does not expose enough texture upload metadata to safely infer multiple mip payloads.

## Validation

- Source-level checks completed for all 18 reported items.
- No Windows compiler, D3D12 SDK runtime, debug layer, or GPU was available here; compilation and runtime validation remain required on Windows.
- No claim is made that the complete engine or other backends have been audited.
