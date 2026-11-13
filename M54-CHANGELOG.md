# M54 — Depth-rejected temporal anti-aliasing

- Added an optional OpenGL temporal resolve after tone mapping, using camera reprojection from current depth and previous view-projection.
- Added persistent color/depth history textures and reset history on renderer resize or detected camera cuts.
- Rejects sky, out-of-screen reprojections and depth-disoccluded history; clamps history to the current 3x3 neighborhood before accumulation.
- Added editor controls for TAA enablement and history weight. TAA takes precedence over FXAA when enabled.
- Scope limitation: this implementation uses camera-only reprojection. Moving objects do not yet provide per-object velocity, so temporal accumulation should be disabled for scenes with substantial object motion until a velocity buffer is added.
