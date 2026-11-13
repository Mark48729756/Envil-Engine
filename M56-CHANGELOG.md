# M56 — Three-cascade directional shadow maps

- Replaced the OpenGL renderer's single sun depth map with a 3-layer `GL_TEXTURE_2D_ARRAY` depth target.
- Added practical logarithmic/uniform split computation and per-cascade frustum fitting from camera slice corners.
- Stabilized each cascade by snapping its light-space center to shadow texels and quantizing the fitted radius.
- Rendered each cascade into its own array layer and culled shadow casters against that cascade's light frustum.
- Added editor controls for shadow distance and split blending.
- Scope: this integration is on the OpenGL renderer. Vulkan/D3D12 RHI retain their previous shadow path in this milestone.
