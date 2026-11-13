<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=gradient&customColorList=12,20,24&height=240&section=header&text=ANVIL%20Engine&fontSize=72&fontColor=ffffff&fontAlignY=38&desc=C%2B%2B20%20%C2%B7%20Deferred%20PBR%20%C2%B7%20OpenGL%20%2F%20D3D12%20%2F%20Vulkan%20RHI&descSize=20&descAlignY=60&animation=fadeIn" alt="ANVIL Engine" width="100%">
</p>

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=500&size=20&pause=1200&color=FF8A1F&center=true&vCenter=true&width=700&lines=Rage+(2008)-class+technology;Physically+based+%26+photoreal+rendering;Built+by+ANVIL+studio" alt="Typing SVG">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/CMake-3.20%2B-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake">
  <img src="https://img.shields.io/badge/OpenGL-4.5-5586A4?style=for-the-badge&logo=opengl&logoColor=white" alt="OpenGL 4.5">
  <img src="https://img.shields.io/badge/Direct3D-12-0078D4?style=for-the-badge&logo=windows&logoColor=white" alt="Direct3D 12">
  <img src="https://img.shields.io/badge/Vulkan-1.4%20WIP-AC162C?style=for-the-badge&logo=vulkan&logoColor=white" alt="Vulkan">
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="MIT License">
</p>

---

## ✨ Features

### 🎨 Rendering
- Deferred renderer: G-buffer, Cook-Torrance GGX PBR, HDR, ACES tonemapping
- Directional sun and up to 32 point lights
- Cascaded shadow maps (3 cascades, PCSS), SSAO, SSR, bloom, FXAA, optional TAA
- Split-sum IBL, procedural atmospheric sky, volumetric clouds, height-aware fog
- Normal, roughness, metallic and AO material maps
- Frustum culling, LOD selection, LRU-managed GPU resource cache

### 🖥️ Graphics APIs

| API | Status |
|---|---|
| OpenGL 4.5 | ✅ Supported |
| Direct3D 12 | 🟡 Initial backend (Windows, `-DANVIL_WITH_D3D12=ON`) |
| Vulkan 1.4 | 🚧 In development |

Rendering goes through an RHI abstraction layer. Shaders are authored once in Vulkan GLSL and cross-compiled to SPIR-V and DXIL.

### 🛠️ Editor
Built with Dear ImGui (docking).
- Hierarchy, Viewport, Inspector, Project, Console and Render Settings panels
- Move / Rotate / Scale gizmos, click picking, undo/redo
- Scene hierarchy with drag-and-drop parenting, JSON scene save/load
- Play / Pause / Step with scene snapshot and restore
- Visual UI Designer (`.anvilui` documents)
- Dark graphite theme

### ⚙️ Physics
- Dependency-free built-in AABB prototype
- Jolt Physics 5.2.0 integration (`-DANVIL_WITH_JOLT=ON`) with a Rigidbody component

### 📦 Assets and systems
- Asset database, OBJ model import, PPM textures
- ECS and a renderer-independent UI runtime
- Prototypes: virtual textures, terrain, vegetation, water, weather, SH probe-based GI, skeletal animation

## 🚀 Building

**Requirements:** C++20 compiler, CMake ≥ 3.20, Python 3 with `jinja2` (`pip install jinja2`), and internet access on first configure (dependencies are fetched via FetchContent).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/sandbox/sandbox        # Windows: build\sandbox\Release\sandbox.exe
```

### CMake options

| Option | Description |
|---|---|
| `-DANVIL_WITH_JOLT=ON` | Enable Jolt Physics |
| `-DANVIL_WITH_D3D12=ON` | Enable the Direct3D 12 backend (Windows only) |
| `-DANVIL_PHYSICS_ONLY=ON` | Build physics and tests only, no window or GL |
| `-DANVIL_ASSET_TESTS_ONLY=ON` | Build tests only, no graphics dependencies |

Run tests with `ctest --test-dir build`.

## 🎮 Controls

| Context | Keys |
|---|---|
| Sandbox | `WASD` move · `Q`/`E` down/up · `Shift` fast · mouse look · `Esc` quit |
| Editor | `RMB` + `WASD` fly camera · `W`/`E`/`R` Move/Rotate/Scale |

**Debug:** `ANVIL_AUTOEXIT=1` exits after 1 second; `ANVIL_SHOT=file.ppm` dumps a frame.

## 📌 Status

Several systems are prototypes and are not yet wired into the renderer. Not yet implemented: a full Vulkan backend, runtime IBL integration, glTF import, ray-traced GI, and production virtual texturing and terrain streaming.

See the `M*-CHANGELOG.md` and `*-QA-REPORT.md` files for per-milestone details and known limitations.

## 📄 License

Released under the [MIT License](LICENSE).

<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=gradient&customColorList=12,20,24&height=100&section=footer" alt="">
</p>Backend` (Jolt or the dependency-free fallback). Add a **Rigidbody** in the editor and press Play.

    cmake -S . -B build -DANVIL_WITH_JOLT=ON          # full engine + editor with Jolt (fetches Jolt 5.2.0)
    cmake -S . -B build -DANVIL_PHYSICS_ONLY=ON -DANVIL_WITH_JOLT=ON   # physics + tests only, no window/GL
    ctest --test-dir build

See `M32-CHANGELOG.md` (includes fixes for scene save/load and the old Jolt adapter). Vulkan, Direct3D 12, IBL/HDR and cascaded shadows are the next stages and are **not** implemented yet.


## M33: RHI v2 foundation (Vulkan 1.4 + Direct3D 12, stage 1 of 5)
RHI now describes swapchains, render passes, pipeline state, push constants and a frame loop; the window can be created without OpenGL; shaders are authored once in Vulkan-GLSL and cross-compiled to SPIR-V and DXIL (`cmake/AnvilShaders.cmake`). The Vulkan and D3D12 backends themselves are **not** implemented yet - see `M33-CHANGELOG.md` for the 5-stage plan.


## M36 update

OpenGL and RHI now share the CPU-side stabilized directional-shadow frustum calculation (`anvil/ShadowMath.h`). RHI applies the explicit-API clip-depth conversion inside that shared helper. Direct3D 12 shader compilation can be enabled with `-DANVIL_COMPILE_D3D12_SHADERS=ON` (requires `glslc`, `spirv-cross`, and `dxc`). Direct3D 12 is intentionally still marked unavailable until its native backend is implemented and validated; DXIL shader compilation alone is not a usable backend. See `M36-CHANGELOG.md`.


## M37: Direct3D 12 backend integration (initial)
A Windows-only D3D12 implementation is now available behind `-DANVIL_WITH_D3D12=ON`. This enables the DXIL shader build and the native D3D12 device, descriptor heaps, graphics PSOs, command lists, resource transitions and GLFW swapchain. See `M37-CHANGELOG.md` for requirements and validation limits. The legacy editor still uses OpenGL; migration of the editor viewport to `RhiRenderer` is a separate integration step.

## UI runtime (M39)

`anvil/UI.h` provides a renderer-independent retained widget tree and deterministic layout pass. Widgets support absolute anchors, horizontal/vertical stack containers, fixed/fill sizing, padding, margins, min/max constraints, recursive ID lookup/removal and hit testing. The UI runtime is intentionally independent of OpenGL/Vulkan/D3D12 so projects can author and test UI documents without a graphics context. Rendering, input/focus routing, serialization and the visual canvas are staged follow-ups.


## M40 — Visual UI Designer foundation

M40 adds a dockable UI Designer to the editor. It provides a widget tree, a scaled 800×450 preview canvas, direct selection and drag positioning, a property inspector, and save/load for versioned `.anvilui` documents under `assets/ui/`. Supported initial widgets are Panel, Button and Label. The preview is editor-side drawing; it is not yet the final runtime UI renderer.

UI documents use the `ANVIL_UI 1` header and are validated when loaded. The dependency-free UI tests cover layout, hit testing, widget ownership and document round-tripping.

See `M40-CHANGELOG.md` for implementation scope and known limitations.


## M41 — Neutral Graphite Editor Theme
- Replaced saturated blue editor highlights with restrained neutral-gray interaction states.
- Unified docked panels, tabs, controls, borders and separators under a low-distraction graphite palette.
- Kept semantic status colors distinct from the base UI theme.
- See `M41-CHANGELOG.md` for the palette and verification scope.


## M44 — D3D12 Compute validation
A native Windows D3D12 compute validation executable and first `.comp` shader now exercise UAV root signature, compute PSO, direct Dispatch, ExecuteIndirect, UAV barriers and CPU readback. See `M44-CHANGELOG.md` for exact build steps and the remaining generic-RHI integration boundary.

## M45 — D3D12 Compute QA hardening

M45 corrects the M44 compute validation test's initial-data/expected-value mismatch, adds checked fence waits and WARP fallback, and registers a portable compute shader contract test. See `M45-CHANGELOG.md` and `D3D12-QA-REPORT-M45.md`. Native D3D12 GPU validation still requires a Windows host.

## M46: Rendering quality + visibility optimization
- Meshes now carry cached local bounding spheres. Both OpenGL and RHI renderers use conservative camera/light frustum culling; RHI avoids preparing resources for objects invisible to both passes and avoids albedo uploads for shadow-only casters.
- The editor reports submitted, camera-visible, camera-culled and shadow-visible object counts.
- Lighting shaders now use a procedural sun-driven sky gradient with horizon haze and a solar disc/glow, plus roughness/view-dependent specular ambient occlusion.
- GL and RHI shader parity is guarded by a shader contract test. Frustum tests cover GL and Vulkan/D3D clip-depth conventions, bounds under shear and invalid-bound fail-open behavior.
- This is a rendering/optimization step, not a claim of complete photorealism. Environment IBL, normal/roughness/metallic maps, TAA, cascaded shadows and ray-traced GI are still outstanding. See `M46-CHANGELOG.md`.

## M47-M49: Photorealism foundations
- M47 adds view-ray Rayleigh/Mie atmospheric integration and 3D volumetric cloud ray marching in the OpenGL and RHI lighting shaders.
- M48 adds compute shader stages for HDR equirectangular conversion, irradiance convolution, GGX prefiltering and BRDF LUT generation. Runtime dispatch/binding is still pending.
- M49 adds OpenGL normal/roughness/metallic/AO material map support and Material references. RHI material-map parity remains pending.
- See `M47-M49-CHANGELOG.md` and `M47-M49-QA-REPORT.md` for scope and limitations.

## M50–M52 — Photorealism and lighting progression

- **M50:** 16-tap rotated Poisson PCF improves filtering for the existing shadow map. Cascaded Shadow Maps still require multi-cascade rendering and renderer-side resource plumbing.
- **M51:** split-sum IBL shader evaluation is present for irradiance, prefiltered environment, and BRDF LUT resources. It remains disabled by default until HDR environment loading, compute preprocessing, and descriptor/uniform binding are connected.
- **M52:** height-aware analytic fog uses approximate optical depth and Beer-Lambert transmittance. Full froxel volumetrics and dynamic weather remain future work.
- QA report: `M50-M52-QA-REPORT.md`.

## M53–M55 — Rendering efficiency and temporal stability
- **M53:** RHI mesh/albedo GPU caches use byte estimates, a configurable budget and LRU eviction; current-frame resources are pinned.
- **M54:** OpenGL adds optional depth-rejected temporal AA with camera reprojection, history clipping and resize invalidation. It is intended for static geometry until per-object velocity is available.
- **M55:** Portable source-contract regression coverage and editor controls for TAA/history weight.
- See `M53-CHANGELOG.md`, `M54-CHANGELOG.md`, `M55-CHANGELOG.md` and `M53-M55-QA-REPORT.md`.


## Recent rendering milestones
- **M56–M58:** OpenGL three-cascade sun shadows, stabilized cascade fitting, PCSS blocker search/soft filtering, editor controls, and portable regression contracts. See `M56-M58-QA-REPORT.md` for validation scope and backend limitations.

## M59–M61 — Large-world foundations
- `engine/include/anvil/WorldOptimization.h`: distance-based LOD selection with hysteresis and asynchronous priority-based asset file IO.
- `shaders/rhi/gpu_occlusion_cull.comp`: conservative previous-frame depth-pyramid occlusion classification compute shader (not yet dispatched by the renderer).
- QA: 12/12 portable contract/unit tests pass. See `M59-M61-QA-REPORT.md` for integration limits and unverified GPU/compiler steps.

## M65–M67: Virtual Textures, Terrain and Vegetation

The engine now includes CPU reference systems for virtual-texture page residency, heightfield terrain sampling/LOD, and deterministic vegetation placement/culling. Vulkan GLSL compute prototypes for feedback compaction and vegetation distance culling are included. See `M65-M67-QA-REPORT.md` for validation and integration limitations.

## M68–M70 — Water, advanced materials and weather foundations
- **M68:** CPU directional-wave reference, analytic normals, Fresnel/shore foam helpers and a Vulkan GLSL refraction/reflection water-pass prototype.
- **M69:** parallax-occlusion reference traversal, normalized material-layer blending, triplanar weights and reusable Vulkan GLSL helpers.
- **M70:** weather-state transitions, rain wetness/snow accumulation model and a Vulkan GLSL precipitation-particle compute prototype.
- QA: 20/20 Debug tests pass. These are engine foundations/prototypes; renderer resource binding, pass scheduling and GPU validation remain outstanding. See `M68-M70-QA-REPORT.md`.

## M71–M73: clouds, diffuse GI and skeletal animation foundations
- `anvil/CloudSystem.h` provides deterministic procedural cloud-density sampling, wind advection and a Beer-Lambert shadow approximation; `shaders/rhi/cloud_volume.frag` is a bounded volumetric integration prototype.
- `anvil/GlobalIllumination.h` evaluates diffuse irradiance from nine real spherical-harmonic coefficients and blends valid probes by distance; `shaders/rhi/gi_sh.frag` evaluates SH lighting from world normals. This is probe-based diffuse GI, not ray tracing.
- `anvil/Animation.h` provides validated skeleton topology, keyframe sampling, quaternion slerp, clip playback/cross-fades and planar two-bone IK; `shaders/rhi/skinning.vert` provides four-weight linear blend skinning.
- Runtime cloud-noise generation/reprojection/shadow injection, GI probe capture and renderer integration, glTF animation import, global pose evaluation, animation graphs/root motion and GPU palette upload remain future work. These shaders are prototypes and are not yet dispatched by the runtime renderer.

## M77–M83 — Gameplay and Editor Foundations

The M77–M83 increment adds standalone C++20 foundations for spatial audio parameter evaluation, branching dialogue, interactable objects, AABB collision queries, deterministic destruction chunks, Verlet distance constraints, and a scene-editor data model. See `M77-CHANGELOG.md` through `M83-CHANGELOG.md` and `M77-M83-QA-REPORT.md` for scope and limitations. These modules are covered by focused dependency-free Debug tests; they are not yet fully connected to the application runtime or platform audio/physics backends.


## M84–M100 — Editor, Rendering Architecture and Release Foundations

The M84–M100 increment adds dependency-free C++20 foundations for terrain sculpting, node graphs, asset classification/import metadata, material and animation editing models, ray-tracing scene metadata, irradiance probe storage, cinematic camera math, render-graph scheduling, profiler history, task dependency graphs, vehicle dynamics, grid A* navigation, gameplay inventory/save snapshots, build manifests, graphics API selection and SDK package validation. See `M84-CHANGELOG.md` through `M100-CHANGELOG.md` and `M84-M100-QA-REPORT.md`. These are focused foundations, not a claim that every subsystem is fully integrated or that ANVIL is already a commercial AAA-ready release.

## M100 Critical Audit Fixes

See `M100-AUDIT-FIX-REPORT.md` for the critical correctness/security fixes and exact validation limits. The dependency-free Debug QA path currently passes 51/51 tests. Full editor/runtime configuration and shader/GPU validation still require the external graphics dependencies and a target machine.


## M103–M105 — Controller, import/migration and Project Hub foundations
- `anvil/Controller.h`: GLFW 3.4 standardized and raw controller state, deadzone/sensitivity/inversion, action/axis bindings, GUID-checked profile persistence and rumble API.
- Editor Controller Debugger: connected device inventory, raw control counts, live button/axis state and rumble test.
- `anvil/Import.h`: PE import-directory/DLL inventory, graphics API hints, APK ZIP metadata, binary Android manifest package/permission/SDK parsing, ABI detection and project asset inventory.
- `MigrationWizard`: transactional copy of selected raw assets from recognized project directories. It does not convert proprietary scenes/materials/animations or reconstruct compiled source projects.
- `ProjectHub`: atomic project creation, validation, discovery, recent-project tracking, multi-target metadata and per-platform configuration files.
- Editor startup Project Hub: create/open/recent and import/migration tabs.
- Per-platform configuration files are metadata only. `build_pipeline=not_configured`; closed platforms require official SDK access.

Dependency-free focused tests (GLFW test double; zlib optional):
```sh
cmake -S . -B build-m103 -DANVIL_M103_M105_TESTS_ONLY=ON
cmake --build build-m103
ctest --test-dir build-m103 --output-on-failure
```
See `M103-M105-CHANGELOG.md` for verification results and the explicit not-implemented list. Full editor/engine compilation still requires the CMake FetchContent dependencies.


## M103–M105 package import and controller API (continuation)

### APK package extraction

`anvil::ImportAnalyzer::analyze("game.apk")` inventories the Android package and parses the binary manifest metadata. `anvil::PackageImporter::importApk(apk, destination, options)` extracts the original archive tree byte-for-byte, including DEX, resources, assets and `lib/<abi>/*.so` files. The importer validates ZIP central-directory bounds, local headers, compression method, CRC32 and relative paths; it rejects encrypted/ZIP64 entries and enforces configurable per-entry and total extraction limits. This is package extraction, **not** DEX decompilation or reconstruction of an editable Android project.

### Windows executable snapshot

`anvil::ImportAnalyzer::analyze("game.exe")` inspects PE32/PE32+ architecture, normal and RVA-based delay-load imports, graphics API DLL hints and embedded PE resources. `anvil::PackageImporter::importWindowsExecutable(exe, destination, options)` creates a staged runtime snapshot containing the EXE, discoverable adjacent imported DLLs, PDB (if present), configuration files, common data folders and extracted embedded resources. Missing or transitive dependencies are reported rather than invented. It does not recover source code, registry/installer state or dependencies outside the supplied directory.

### Controller input

The GLFW 3.4 backend exposes standardized gamepad state and raw physical buttons, axes and hats. GUIDs are retained and SDL-compatible GUIDs are decoded for VID/PID when encoded. `ControllerSystem::loadGamepadMappings(path, &loaded, &error)` loads SDL-compatible controller mapping database lines through GLFW, expanding standardized mapping coverage where mappings exist. `ControllerSystem::notifyKeyboardMouseActivity()` allows the host input callbacks to switch the active input family; controller polling switches it back when physical gamepad input is detected. Profiles remain GUID-checked. Transport type, battery and vendor-specific extensions remain unknown unless a future native backend can verify them. `glfwSetJoystickRumble` is used for the two-motor rumble API; HD haptics and DualSense adaptive triggers are not implemented.

### Focused validation build

```sh
cmake -S . -B build-m103 -DANVIL_M103_M105_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-m103 -j2
ctest --test-dir build-m103 --output-on-failure
```

The focused build compiles the importer and controller tests without downloading the editor's graphics dependencies. A full editor/engine build still requires the CMake FetchContent dependencies (GLFW, GLM, GLAD, ImGui and NanoSVG).

- **M107:** optional Steam Input backend, controller mapping database loader, device metadata, hats and active-input tracking — see `M107-STEAM-INPUT-ADVANCED-CONTROLLERS.md`.

## M108 — Controller remapping and input prompts

M108 adds per-controller button remapping (including unbinding), Xbox/PlayStation/Nintendo/generic prompt labels, and profile v3 persistence while preserving v1/v2 profile compatibility. Raw physical button state remains available for diagnostics and rebinding screens. See `M108-CHANGELOG.md` and `M108-QA-REPORT.md`.
