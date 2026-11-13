# ANVIL ENGINE — M39: retained UI runtime foundation

## Added
- Renderer-independent retained UI widget tree (`anvil/UI.h`).
- Widget types for panels, labels, buttons, images and custom controls.
- Stable runtime widget IDs, recursive lookup and ownership-safe child removal.
- Absolute anchor layout with offsets, padding, margins and min/max constraints.
- Horizontal and vertical stack layouts with fixed and fill sizing.
- Reverse-order, deepest-child hit testing with visibility, enabled-state and optional parent clipping.
- Dependency-free layout regression tests, included in both asset-only and full CMake test configurations.

## Verification
- Built `anvil_ui_layout_tests` with GCC 14.2 in C++20 mode and `-Wall -Wextra -Wpedantic -Werror`.
- Ran the three-test CTest suite in `ANVIL_ASSET_TESTS_ONLY` mode: Asset Database, RHI selection and UI layout all passed (3/3).

## Scope
This milestone is the runtime/layout foundation, not yet a complete UI renderer or visual editor. Text shaping, font assets, GPU batching, pointer/focus event routing, themes, serialization, responsive preview and the drag-and-drop UI canvas remain subsequent work.
