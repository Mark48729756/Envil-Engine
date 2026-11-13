# ANVIL Engine M100 — Critical Audit & Fix Report

Date: 2026-11-12
Base: M84–M100 project archive (contains the accumulated source tree through M100).

## Defects found and fixed

1. **Render Graph resource hazards:** the compiler tracked RAW and WAW dependencies but missed WAR (read-before-overwrite) dependencies. Added reader tracking and deduplicated pass dependencies so a later write cannot be scheduled before earlier readers.
2. **Package path traversal:** build and SDK manifests rejected POSIX `../` paths but did not reject Windows `..\\` traversal, drive paths, or alternate-stream syntax. Added shared cross-platform relative-path validation.
3. **World Partition integer safety:** non-finite/out-of-range world coordinates could be converted to `int`; extreme radii could cause overflow and enormous loops. Added finite/range checks, bounded radii and wide integer distance arithmetic.
4. **Async streamer resource exhaustion:** unbounded file reads could allocate arbitrary memory. Added a configurable maximum asset size (512 MiB default), stream-size checks, and a completed-but-not-yet-polled deduplication state.
5. **Async streamer startup race:** the worker thread could start before its maximum-size setting was initialized due to member declaration order. Moved the worker to the end of member initialization.
6. **Vehicle braking:** applying a fixed braking impulse could reverse a slowly moving vehicle. Braking is now clamped so it cannot cross through zero speed in one step.
7. **Navigation and probe-volume dimensions:** multiplication/index-range overflow could produce invalid allocations or indexing. Added checked dimension calculations and safe invalid-grid behavior.
8. **Non-finite input handling:** added finite/range validation to terrain editing/sampling, animation track sampling, material parameter ranges, cinematic CoC, weather, water, clouds, character controller, constraint solver, ragdoll, audio, interactions, collision bounds, destruction and profiler statistics.
9. **Graph traversal depth:** node and task graph cycle/order checks used recursive traversal and could overflow the call stack on deep graphs. Replaced with iterative Kahn topological traversal.
10. **Virtual texture feedback:** non-finite priorities could violate sort comparator requirements; mip increment and LRU counter wraparound were not guarded. Inputs are sanitized and counters renormalized safely.
11. **API selection:** when no API was reported available, the selector silently returned OpenGL. It now returns `GraphicsApi::Unknown` so callers can handle the no-backend case explicitly.
12. **Game state inventory:** adding item counts could overflow signed `int`; loading a malformed snapshot could introduce negative counts. Both are guarded.

## Validation performed

- Debug CMake configuration for `ANVIL_ASSET_TESTS_ONLY=ON`: PASS.
- C++ test build: PASS.
- CTest: **51/51 PASS**.
- Targeted AddressSanitizer + UndefinedBehaviorSanitizer executables for 11 critical modules: **11/11 PASS**.
- ZIP integrity check: PASS.
- Build/test/temp directories are not included in the deliverable archive.

## Known validation limits (not represented as passes)

- Full editor/runtime CMake configuration was attempted but could not download GLFW because network DNS could not resolve `github.com` in this environment.
- `glslangValidator`, `glslc`, `spirv-val` and `dxc` are unavailable; shader compilation was not performed.
- No native GPU, D3D12, Vulkan or OpenGL runtime test was possible.
- This audit fixes concrete defects found in the inspected source and expands regression coverage; it is not a mathematical proof that no further defects exist in the entire engine.
- Several M1–M100 systems remain CPU-side foundations or shader prototypes as documented in their respective changelogs. This archive does not claim that all such systems are fully integrated into the editor/runtime.
