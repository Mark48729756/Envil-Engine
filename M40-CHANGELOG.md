# ANVIL ENGINE — M40: Visual UI Designer Foundation

## Added
- Integrated a dockable UI Designer window into the editor.
- Widget hierarchy panel with selection.
- 800 × 450 responsive preview canvas with scaled widget rendering.
- Select widgets directly in the preview and drag them to reposition.
- Add Panel, Button and Label widgets; delete non-root widgets.
- UI Inspector for widget name, text, position, size, margins, colors, corner radius, visibility and enabled state.
- Save/load UI documents at `assets/ui/main.anvilui`.
- Versioned deterministic UI document serialization (`ANVIL_UI 1`) with quoted UTF-8 text fields, hierarchy reconstruction, enum/depth validation and malformed-document rejection.
- Default sample UI document generated in memory for first launch.

## Verification
- `ANVIL_ASSET_TESTS_ONLY=ON` CMake build completed with GCC.
- CTest: 3/3 passed (Asset Database, RHI Selection, UI layout/document round-trip).
- Full editor/MSVC build has not yet been verified in this environment.

## Scope and known limitations
- The preview currently draws widget rectangles and text with ImGui; it is an editor preview, not the runtime GPU UI renderer.
- Canvas dragging edits absolute-layout offsets. Nested visual editing, drag-and-drop reparenting, resize handles, snapping/guides and undo/redo are not implemented yet.
- Font assets, text shaping, image widget rendering, pointer/focus event dispatch and runtime UI input remain future milestones.
