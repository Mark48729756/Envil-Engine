# M72 — Spherical-harmonic global illumination foundations

- Added `GlobalIllumination.h`: nine-coefficient real spherical-harmonic diffuse irradiance evaluation and validity/distance-weighted probe blending.
- Added `shaders/rhi/gi_sh.frag`: evaluates SH9 irradiance from world-space normals.
- Probe capture, update scheduling, spatial acceleration, specular GI and renderer binding are not yet integrated; this is diffuse probe GI, not ray-traced/path-traced GI.
