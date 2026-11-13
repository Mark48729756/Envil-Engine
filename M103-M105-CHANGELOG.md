# ANVIL Engine — M103–M105 continuation changelog

## M103 — Controller input foundation
- Retained the GLFW 3.4 backend and added both standardized gamepad state and raw physical button/axis/hat snapshots. Generic HID hat directions are mapped to the standard D-pad actions.
- Added per-axis deadzone, sensitivity and inversion processing with bounded input values.
- Added action-button and axis-action bindings.
- Added GUID-checked controller profile persistence; stored axis settings are remapped to the currently connected joystick slot when loading.
- Added a live Controller Debugger panel to the editor with device GUID, mapping, physical counts, button state, axis meters and manual rumble test/stop.
- Decodes VID/PID from SDL-compatible GUIDs when those fields are present and applies a small VID-to-manufacturer lookup; otherwise these fields remain unknown. USB/Bluetooth transport, battery and vendor-specific capabilities remain unreported because GLFW does not expose them.
- Added SDL-compatible GLFW gamepad mapping database loading, explicit keyboard/mouse activity notification and active-input-device state so the host editor/game can switch control hints when its keyboard/mouse callbacks call `notifyKeyboardMouseActivity()` and controller polling detects physical input.

## M104 — Import analysis and asset migration
- PE analyzer reads PE32/PE32+ machine architecture, maps section RVAs, enumerates normal and RVA-based delay-load DLL imports, and detects Direct3D, Vulkan and OpenGL DLL families by imported names.
- PE resource-directory traversal extracts bounded embedded resources (including RCDATA, icons, manifests and version resources) byte-for-byte under `Resources/PE/`. Named PE resources are retained using generated directory identifiers where a numeric resource ID is unavailable.
- EXE bundle import copies the executable, discoverable imported DLLs beside it, matching PDB symbols, adjacent configuration files and common runtime data folders (`Data`, `Assets`, `Content`, `Plugins`). Missing dependencies are reported; transitive/system dependencies are not fabricated.
- ZIP/APK analyzer records compression metadata, local-header offsets, native library paths and ABI directory names.
- Added bounded APK package extraction preserving the archive directory tree, DEX files, resources, assets and native ABI libraries. Each extracted entry is checked against its central-directory CRC; encrypted and ZIP64 entries are rejected explicitly, unsafe paths are skipped, and configurable per-entry/total size limits protect against path traversal and decompression bombs.
- Added binary Android XML string-pool and start-element decoding for package name, permissions, min SDK and target SDK. APK extraction is byte-preserving: it does not decompile DEX or convert Android resources into editable ANVIL assets.
- Added project-directory asset inventory for textures, models, audio, animation, material and scene-like files.
- Added transactional directory migration: copies selected supported raw asset files into a staging directory, writes a migration report and commits by rename. Scene conversion remains opt-in and not implemented; it is skipped by default.

## M105 — Project Hub and platform metadata
- Project creation now uses a staging directory and atomic rename, creates a standard asset/config/script/build folder layout and writes project metadata plus README.
- Supports multiple target platform selections and creates per-platform configuration files. Closed-console configurations explicitly require official SDKs; build pipelines are marked `not_configured` rather than simulated. The Blank/2D/3D template names currently select project metadata and starter folder layout, not full scene/content templates.
- Added project validation, discovery, sorted recent projects and a bounded recent list.
- Added a startup Project Hub window in the editor for create/open/recent workflows and an Import/Migration tab. The tab now exposes APK package extraction and Windows EXE runtime-bundle import actions in addition to project-directory asset migration.

## Verification
- C++20 isolated build: `cmake -S . -B build-m103 -DANVIL_M103_M105_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug`
- Build: `cmake --build build-m103 -j2`
- Tests: `ctest --test-dir build-m103 --output-on-failure`
- Result in this environment: **2/2 tests passed** (controller GUID metadata, raw buttons/axes/hats, deadzone/inversion, action/axis mapping, rumble and profile persistence; PE normal imports, architecture, embedded-resource extraction, EXE bundle copying; APK binary manifest, deflate/stored extraction, CRC validation, path traversal rejection; Project Hub and directory migration).
- Full engine CMake configuration was attempted. It is blocked before generation because FetchContent cannot resolve `github.com` to download GLFW 3.4. The editor translation unit and full engine were therefore not compiled here.

## Still not implemented / not verified
- Native XInput/Raw Input/Windows HID/Steam Input/Android NDK backends. The currently integrated GLFW backend provides standardized gamepad state plus generic raw buttons, axes and hats on platforms supported by GLFW.
- Vendor protocols for Joy-Con/Steam Deck/DualSense, adaptive triggers, HD haptics, touchpad, gyro/accelerometer, speaker, microphone, RGB/player LEDs and battery level. These require native platform APIs or vendor/Steam SDK integration and are not advertised as working.
- Hot-plug callback integration, keyboard/mouse action mapping, per-device UI remap wizard, controller profile editor and automatic platform-specific glyph atlas. Host code must currently notify keyboard/mouse activity explicitly.
- APK DEX decompilation, Android resource-table decoding/translation, source-project reconstruction, Android signing/build pipeline and runtime ABI compatibility validation. APK files are extracted intact; extraction is not project recovery.
- Transitive DLL dependency resolution, PDB parsing/symbol indexing, installer/registry state recovery, and executable sandboxing. PDB files are copied when present but not parsed.
- Unity/Unreal/Godot serialized asset conversion, material/shader conversion, animation conversion and scene/level conversion. Current migration copies selected raw files only.
- ZIP-based arbitrary project migration and project-format-aware scene/material/animation conversion. APK import has its own safe ZIP extraction path.
- Per-platform build pipelines, packaging, signing and deployment. PlayStation/Xbox/Nintendo Switch require licensed access and official SDKs.
- Android virtual controls/layout editor and mobile touch input.
- Full graphical import compatibility matrix and migration undo/redo journal.

No unsupported capability above is advertised as working. The controller debugger and Project Hub source changes are integrated into `editor/main.cpp`, but editor compilation/runtime validation is pending the unavailable FetchContent dependencies.

## Importer capability matrix (this continuation)

| Input | Implemented and tested | Explicitly not claimed |
|---|---|---|
| APK | ZIP central-directory validation, binary manifest metadata, ABI/library inventory, byte-preserving extraction, CRC/path/size checks | DEX decompilation, resource-table conversion, source recovery, signing/building |
| EXE | PE32/PE32+ architecture, normal + RVA delay imports, graphics DLL hints, embedded resource byte extraction, sibling DLL/config/PDB/data snapshot | Transitive/system DLL resolution, installer/registry capture, source recovery, execution sandbox |
| Controllers | GLFW standardized + raw buttons/axes/hats, GUID-derived VID/PID when encoded, action/axis mapping, profiles, two-motor rumble API | Vendor extensions, Steam Input, XInput/Raw Input direct APIs, Android native input, sensors, adaptive triggers, battery, USB/Bluetooth transport |

`PackageImporter::importApk()` and `PackageImporter::importWindowsExecutable()` never overwrite an existing destination. Both stage into a sibling temporary directory and commit by rename; a failed import removes the staging directory.
