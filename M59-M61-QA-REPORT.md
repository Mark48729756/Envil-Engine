# ANVIL Engine M59–M61 QA report

## Delivered
- M59: Vulkan GLSL 4.50 compute shader for conservative previous-frame depth-pyramid occlusion classification; shader contract test added.
- M60: backend-agnostic distance LOD selector with configurable thresholds and hysteresis.
- M61: asynchronous priority-based file streamer with pending/active duplicate suppression and completion polling. File IO runs on a worker; GPU upload is intentionally left to the render thread.

## Automated validation
- Configure: `cmake -S . -B build-qa -DANVIL_ASSET_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON` — passed.
- Build: `cmake --build build-qa -j2` — passed.
- CTest: **12/12 passed**.
- ZIP integrity: verified with `unzip -t` after packaging.

## Not verified / not yet integrated
- No GLSL compiler (`glslangValidator` / `glslc`) was available, so the new compute shader has a source-contract test only; it was not compiled to SPIR-V.
- No GPU runtime test or performance/FPS measurement was performed.
- M59 compute dispatch, previous-frame depth-pyramid generation, synchronization/barriers and indirect draw consumption are not wired into Renderer/RhiRenderer.
- M60 LOD selection is a reusable CPU utility; model LOD import, per-entity LOD sets and draw submission are not yet connected to the editor/renderers.
- M61 performs asynchronous CPU file reads only. Scene-cell streaming, asset eviction and render-thread GPU upload integration remain future work.
