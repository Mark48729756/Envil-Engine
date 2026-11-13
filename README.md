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
  <img src="https://img.shields.io/badge/Vulkan-1.4-AC162C?style=for-the-badge&logo=vulkan&logoColor=white" alt="Vulkan">
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
- Render graph scheduling and GPU profiler

### 🖥️ Graphics APIs

| API | Status | CMake option |
|---|---|---|
| OpenGL 4.5 | ✅ Supported | default |
| Vulkan 1.4 | 🟡 RHI backend, opt-in | `-DANVIL_WITH_VULKAN=ON` |
| Direct3D 12 | 🟡 RHI backend, Windows only | `-DANVIL_WITH_D3D12=ON` |

Rendering goes through an RHI abstraction layer. Shaders are authored once in Vulkan GLSL and cross-compiled to SPIR-V and DXIL. The editor viewport still uses the OpenGL renderer.

### 🛠️ Editor
Built with Dear ImGui (docking).
- Hierarchy, Viewport, Inspector, Project, Console and Render Settings panels
- Move / Rotate / Scale gizmos, click picking, undo/redo
- Scene hierarchy with drag-and-drop parenting, JSON scene save/load
- Play / Pause / Step with scene snapshot and restore
- Visual UI Designer (`.anvilui` documents)
- Project Hub: create, open and recent projects
- Controller Debugger with rebinding and rumble test
- Dark graphite theme

### ⚙️ Physics
- Dependency-free built-in AABB prototype
- Jolt Physics 5.2.0 integration (`-DANVIL_WITH_JOLT=ON`) with a Rigidbody component

### 🎬 Animation
- Skeleton to skinning-palette pipeline, animation events, root motion
- glTF / GLB importer: skeletons, clips (linear, step, cubic spline) and skinned meshes
- Four-weight GPU skinning shader (renderer integration in progress)

### 🎮 Input
- GLFW gamepads, Windows XInput, Linux evdev and Android backends
- Button remapping, per-platform prompt labels, GUID-bound profiles, rumble
- Optional Steam Input (`-DANVIL_WITH_STEAM_INPUT=ON`, requires the Steamworks SDK)

### 📦 Assets and systems
- Asset database, OBJ and glTF import, PPM textures
- Import and migration tools for APK and Windows executable packages (raw asset extraction)
- ECS and a renderer-independent UI runtime
- Foundations: terrain, vegetation, water, weather, virtual textures, SH probe GI, navigation, vehicles, dialogue, audio parameters, destruction

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
| `-DANVIL_WITH_VULKAN=ON` | Enable the Vulkan backend (requires the Vulkan SDK) |
| `-DANVIL_WITH_D3D12=ON` | Enable the Direct3D 12 backend (Windows only) |
| `-DANVIL_COMPILE_D3D12_SHADERS=ON` | Compile shaders to DXIL (`glslc`, `spirv-cross`, `dxc`) |
| `-DANVIL_WITH_STEAM_INPUT=ON` | Enable Steam Input (requires the Steamworks SDK) |
| `-DANVIL_PHYSICS_ONLY=ON` | Build physics and tests only, no window or GL |
| `-DANVIL_ASSET_TESTS_ONLY=ON` | Build dependency-free tests only |
| `-DANVIL_M103_M105_TESTS_ONLY=ON` | Build import and controller tests only |

Run tests with `ctest --test-dir build`.

Shaders are located at runtime via `ANVIL_SHADER_DIR`, then `<exe dir>/shaders`, then `./shaders`.

## 🎮 Controls

| Context | Keys |
|---|---|
| Sandbox | `WASD` move · `Q`/`E` down/up · `Shift` fast · mouse look · `Esc` quit |
| Editor | `RMB` + `WASD` fly camera · `W`/`E`/`R` Move/Rotate/Scale |

**Debug:** `ANVIL_AUTOEXIT=1` exits after 1 second; `ANVIL_SHOT=file.ppm` dumps a frame.

## 🗺️ Roadmap

| Area | Next |
|---|---|
| Animation | GPU skinning in `Renderer` / `RhiRenderer`, animation graph v2, IK, motion matching, FBX import |
| Rendering | Runtime IBL, GPU occlusion culling, ray-traced GI |
| World | Virtual texturing and terrain streaming in production form |

## 📌 Status

Many systems are foundations or prototypes and are not yet wired into the renderer. Per-milestone details and known limitations are in the `M*-CHANGELOG.md` and `*-QA-REPORT.md` files.

## 📄 License

Released under the [MIT License](LICENSE).

<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=gradient&customColorList=12,20,24&height=100&section=footer" alt="">
</p>g.
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
