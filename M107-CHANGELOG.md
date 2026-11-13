# M107 — Steam Input and advanced controller plumbing

- Added optional `ANVIL_WITH_STEAM_INPUT` CMake integration; Steamworks remains absent from default builds.
- Added Steam Input manifest/action-set initialization, digital and analog action bindings, controller enumeration, normalized state, dual-motor vibration, trigger rumble, and fused motion-data access.
- Steam action names accept short manifest names or fully-qualified paths; action sets accept short names or `/actions/...` paths.
- Added SDL GameControllerDB mapping-file loader using GLFW's mapping API.
- Added best-effort vendor/product/manufacturer metadata, raw hat state, and keyboard/mouse-vs-gamepad activity tracking API.
- Corrected SDL GUID product-ID byte offset in Linux duplicate matching and metadata extraction.
- Kept non-Steam controller backends operational when Steam Input is not compiled in or cannot initialize.
