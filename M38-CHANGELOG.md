# ANVIL ENGINE — M38: camera motion blur foundation

## Added
- Optional depth-reprojected camera-motion blur in the legacy OpenGL post-processing pass.
- Previous-frame view-projection history with safe initialization and resize reset.
- Motion-blur enable/strength controls in the editor Render Settings panel.
- `M38-ROADMAP.md` describing the staged rendering, UI authoring, IDE, editor, and production-pipeline work needed to approach a Godot-class workflow.

## Limitations
- This is camera-motion blur only. Independently moving objects need previous transforms and a velocity buffer.
- The new effect is wired only into the legacy OpenGL `Renderer`; it is not yet wired into `RhiRenderer`, Vulkan, or D3D12.
- The embedded text editor is an initial editing surface only: syntax highlighting, completion, diagnostics, language-server integration and debugging are not implemented yet.
- The graphics/editor targets were not compiled in this Linux test pass. Only the dependency-free Asset Database and RHI selection test targets were built and run.
