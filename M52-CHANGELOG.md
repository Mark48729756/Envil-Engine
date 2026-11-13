# M52 — Height-aware atmospheric fog

- Replaced the simple distance-only fog blend with a low-cost height-dependent density approximation in both OpenGL and RHI lighting shaders.
- Integrates camera and fragment height into an approximate optical depth and Beer-Lambert transmittance.
- Uses a bounded optical depth to avoid numerical overflow and preserves the existing fog enable switch.
- This is an analytic height-fog approximation, not a froxel-based volumetric fog simulation. Rain particle simulation, wet-surface material controls, weather transitions, and temporal reprojection remain future work.
