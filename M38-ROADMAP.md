# ANVIL ENGINE M38 — Rendering and editor roadmap

## M38 implementation in this archive

- Added an optional camera-motion blur post-process to the legacy OpenGL renderer.
- The effect reconstructs world position from the G-buffer depth and reprojects it through the previous frame's view-projection matrix.
- Added editor controls for enabling the effect and adjusting its strength.
- Temporal history is reset when the render target is resized.
- The effect is disabled by default.

### Scope and limitations

This is camera-motion blur, not full per-object motion blur. Moving objects with a stationary camera do not yet produce independent velocity vectors. Correct object blur requires previous model transforms and a velocity buffer (or equivalent per-object motion data).

The M38 effect currently belongs to `Renderer` (legacy OpenGL path). It is not wired into `RhiRenderer`, Vulkan, or D3D12. The D3D12 RHI remains an initial backend integration and is not the editor's active renderer.

## Roadmap toward a Godot-class workflow

The goal is feature coverage and a coherent production workflow, not a superficial feature checklist.

### 1. Rendering foundation
- [x] HDR lighting, ACES tonemapping, bloom, FXAA, SSAO, screen-space reflections, directional shadows.
- [x] Optional camera-motion blur (M38, OpenGL renderer).
- [ ] Per-object velocity buffer and full motion blur.
- [ ] Temporal anti-aliasing with history rejection and camera jitter.
- [ ] Image-based lighting: environment cubemaps, irradiance, prefiltered specular, BRDF LUT.
- [ ] Cascaded shadow maps, contact shadows, improved PCF/PCSS.
- [ ] Color grading, depth of field, chromatic aberration, film grain, vignette.
- [ ] GPU-driven culling, instancing, render graph, GPU profiling.
- [ ] One shared post-processing feature contract across OpenGL, Vulkan and D3D12.

### 2. Visual editor and UI authoring
- [x] Renderer-independent retained widget tree, absolute anchors, horizontal/vertical stack layout, fixed/fill sizing, margins, padding, min/max constraints and hit testing (M39 foundation).
- [ ] GPU UI renderer, text shaping, font assets, input/focus routing, themes and accessibility.
- [ ] Visual UI canvas with hierarchy, widget palette, inspector, anchors and constraints.
- [ ] Drag-and-drop layout editing, live preview at multiple resolutions and DPI scales.
- [ ] UI serialization, reusable components/prefabs, theme assets and data binding.
- [ ] Undo/redo and prefab overrides for UI documents.

### 3. Integrated programming environment
- [ ] Script resource model and language-server boundary.
- [x] Initial embedded project-file text editor with open/save and project-root path confinement (M38; no syntax highlighting yet).
- [ ] Production code editor with syntax highlighting, diagnostics, autocomplete, go-to-definition and refactoring.
- [ ] Build/run tasks, integrated console, debugger, breakpoints, call stack and variable inspection.
- [ ] Hot reload with safe state migration.
- [ ] Shader editor with compile diagnostics and live preview.
- [ ] Start with C++ and shader workflows; add a beginner-friendly engine scripting language only after a stable runtime API exists.

### 4. Editor architecture and extensibility
- [ ] Dockable workspace, scene hierarchy, inspector, asset browser and project manager.
- [ ] Command system, contextual actions, undo/redo transactions and editor preferences.
- [ ] Import pipeline for glTF 2.0, textures, audio, fonts and materials.
- [ ] Resource UID system, dependency graph, incremental reimport and cache invalidation.
- [ ] Plugin API with versioning, permissions and documented extension points.
- [ ] Templates, examples, documentation browser and asset package format.

### 5. Runtime and production pipeline
- [ ] Scene/prefab inheritance and stable serialization IDs.
- [ ] Animation state machines, navigation, audio mixer and input action maps.
- [ ] Deterministic build/export presets and platform packaging.
- [ ] Crash reporting, logs, automated regression scenes and performance budgets.
- [ ] Continuous integration on Windows, Linux and macOS; backend-specific GPU tests.

## Architectural rule

Keep editor-only dependencies out of runtime engine modules. The runtime owns rendering, resources, scenes, UI and scripting APIs; the editor consumes those APIs and supplies authoring tools. This keeps exported games small and makes the editor replaceable.

Godot's own architecture similarly separates the editor from lower-level runtime systems and provides an integrated script editor, visual profiler, import/export tools and GUI authoring. See the official [editor development guide](https://docs.godotengine.org/en/stable/engine_details/editor/introduction_to_editor_development.html) and [feature list](https://docs.godotengine.org/en/4.7/about/list_of_features.html).
