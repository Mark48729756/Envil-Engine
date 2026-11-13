# M67 — Vegetation Placement and Culling Core

- Added deterministic seeded vegetation instance generation with stable position, scale and type selection.
- Added CPU distance and density filtering for visible instance lists.
- Added a Vulkan GLSL distance-culling prototype (`shaders/rhi/vegetation_cull.comp`).
- GPU compaction, indirect draws, wind animation, shadows and terrain/biome placement remain integration work.
