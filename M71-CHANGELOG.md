# M71 — Procedural volumetric cloud foundations

- Added `CloudSystem.h`: deterministic procedural cloud-density sampling, wind advection, cloud-layer bounds and Beer-Lambert cloud-shadow approximation.
- Added `shaders/rhi/cloud_volume.frag`: bounded 48-step volumetric cloud integration with extinction/transmittance and sun-tinted scattering.
- Cloud noise texture generation, temporal reprojection, shadow-map injection and runtime render-pass scheduling remain integration work.
