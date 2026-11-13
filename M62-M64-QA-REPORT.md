# ANVIL Engine M62–M64 QA report

## Delivered
- M62: Vulkan GLSL compute shader prepares one indexed-indirect command per visibility flag, suppressing hidden objects with zero instances.
- M63: LOD transition helper returns primary/secondary LOD IDs and blend weight for renderer-side cross-fade/dither.
- M64: world partition planner computes 3D grid cell load requests and uses a wider unload radius for hysteresis.

## Automated validation
- Portable QA configuration: `cmake -S . -B build-qa -DANVIL_ASSET_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`.
- Debug configuration and build completed successfully. CTest: **13/13 passed** (assertions active).

## Not verified / integration remaining
- No GLSL compiler or GPU execution is assumed; the indirect compute shader receives a source-contract test only.
- The renderer does not yet dispatch the indirect command preparation pass or submit its output as an indirect draw buffer.
- LOD transitions are CPU-side metadata; actual mesh cross-fade/dither and model LOD import are not connected.
- World partition returns cell coordinates only; it is not yet connected to scene-cell serialization, asset streamer scheduling, eviction or GPU uploads.
