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
</p>
