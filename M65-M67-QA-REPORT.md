# ANVIL Engine M65–M67 QA report

## Delivered
- M65: bounded virtual-page residency cache, deduplicated prioritized feedback, LRU eviction and parent-mip fallback; Vulkan GLSL feedback compaction prototype.
- M66: validated CPU heightfield, bilinear sampling, finite-difference normals and distance-based LOD morph selection.
- M67: deterministic vegetation instance generation, CPU distance/density culling and Vulkan GLSL distance-culling prototype.

## Automated validation
- Configure: `cmake -S . -B build-qa -DANVIL_ASSET_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.
- Build: successful.
- CTest: **16/16 passed** with Debug assertions enabled.

## Not verified / integration remaining
- No GLSL compiler was available; the new `.comp` files were not compiled to SPIR-V.
- No GPU execution or visual/FPS test was performed.
- Virtual texture CPU cache is not yet wired to GPU page tables, atlas uploads or shader sampling.
- Terrain CPU heightfield/LOD is not yet connected to a GPU terrain renderer or editor terrain tools.
- Vegetation CPU placement/culling and shader prototype are not yet connected to renderer instance buffers, indirect draw submission, wind or shadows.
