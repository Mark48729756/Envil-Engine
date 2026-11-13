# ANVIL ENGINE — M41: Neutral Graphite Editor Theme

## Changed
- Reworked the Dear ImGui editor palette into a neutral graphite theme inspired by the restrained dark workspaces used by Unreal Engine and Unity.
- Removed saturated blue selection, toolbar, docking-preview and tab accents; active states now use controlled neutral grays.
- Standardized window, child, popup, frame, border, separator, tab, scrollbar, resize-grip and navigation colors.
- Reduced corner radii for a flatter desktop-editor appearance.
- Kept semantic status colors (for example Play/Stop state) separate from the base interface palette.

## Theme palette
- Main editor background: `#242424`
- Docked child panels: `#202020`
- Popups: `#292929`
- Controls: `#343434`
- Hovered controls: `#414141`
- Active controls: `#494949`–`#555555`
- Borders and separators: `#3A3A3A`
- Primary text: `#D0D0D0`
- Secondary text: `#858585`

## Verification
- Configured and built the dependency-free test targets with GCC using `ANVIL_ASSET_TESTS_ONLY=ON`.
- CTest: 3/3 passed (Asset Database, RHI Selection, UI layout/document round-trip).
- The full editor target was not compiled in this environment; the Dear ImGui/GLFW graphics dependencies are fetched during the normal CMake configure.
