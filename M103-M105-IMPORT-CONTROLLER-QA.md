# ANVIL Engine — M103/M104 import + controller continuation QA

## Implemented in source

### APK
- `ImportAnalyzer::analyze()` validates the ZIP EOCD and central-directory bounds, inventories package entries, ABI directories and native `.so` paths, and parses the binary Android manifest metadata.
- `PackageImporter::importApk()` extracts the original package tree byte-for-byte into a staging directory, including DEX, `res/`, `resources.arsc`, `assets/`, `lib/<abi>/` and other package files unless disabled by options.
- Stored ZIP entries work without zlib. Deflate entries are supported when CMake finds zlib.
- CRC32 is checked for extracted entries. Encrypted entries and ZIP64 packages are explicitly rejected. Unsafe absolute/traversal/drive-qualified paths are skipped. Per-entry and total extracted byte limits are configurable.
- Existing destination directories are never overwritten. Failed imports clean up staging output.
- This does not decompile DEX, decode Android resource tables into editable ANVIL assets, recover source code or create a signed APK.

### Windows EXE / PE
- PE32 and PE32+ machine type and architecture detection.
- Normal import descriptors and RVA-based delay-load descriptors are inspected for DLL names.
- Graphics DLL hints are detected for Direct3D, Vulkan and OpenGL families.
- PE resource-directory traversal extracts bounded embedded resource payloads to `Resources/PE/` (raw bytes; no format conversion).
- `PackageImporter::importWindowsExecutable()` stages the EXE, found sibling imported DLLs, matching PDB if present, adjacent common configuration files, common runtime data directories and embedded resources.
- Missing dependencies are reported. Transitive/system dependencies, installers, registry state and source code are not recovered. PDB is copied but not parsed.

### Project Hub integration
- Added APK extraction and Windows EXE runtime-bundle actions to the existing Project Hub Import/Migration tab, alongside the directory migration action. The tab can analyze file/package metadata before import.
- The Project Hub still does not have a native file-picker in this pass; the source and destination paths are entered into the existing path fields.

### Controllers
- GLFW 3.4 standardized gamepad API plus raw physical buttons, axes and hats.
- GUID retention and VID/PID decoding for SDL-compatible GUIDs where those fields are present; manufacturer name is a VID lookup, not hardware interrogation.
- Loads SDL-compatible gamepad mapping database files through `glfwUpdateGamepadMappings()`.
- Deadzone, sensitivity, inversion, action/button mapping, axis mapping, GUID-checked profiles, generic D-pad hat mapping and GLFW two-motor rumble API.
- Active input family switches to gamepad when physical input is observed; host keyboard/mouse callbacks can call `notifyKeyboardMouseActivity()` to switch back.
- The tests use a deterministic GLFW test double; no physical controller was attached in this environment.

## Verification performed

| Configuration | Result |
|---|---|
| C++20 focused build with `-Wall -Wextra -Wpedantic -Werror` | Passed, no compiler warnings |
| Focused CTest | 2/2 passed |
| AddressSanitizer + UndefinedBehaviorSanitizer | 2/2 passed |
| Focused build with zlib disabled | 2/2 passed; stored ZIP extraction path covered |
| APK tests | Synthetic APK manifest, deflate/stored entries, ABI library, assets, CRC failure, traversal path, extraction options |
| PE tests | Synthetic x86_64 PE, DLL import, Direct3D hint, embedded RCDATA resource, adjacent DLL/config/assets bundle |
| Controller tests | Mapping DB load, GUID VID/PID decode, buttons/axes/hats, deadzone/inversion, action/axis mapping, rumble call, profile persistence, input-family switching |

Commands:

```sh
cmake -S . -B build-m103 -DANVIL_M103_M105_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic'
cmake --build build-m103 -j2
ctest --test-dir build-m103 --output-on-failure
```

## Full engine build limitation

A full CMake configure was attempted. It stopped during FetchContent because this environment could not resolve `github.com` to download GLFW 3.4. The complete editor/engine target and hardware integrations therefore remain unverified in this run. The focused M103/M104 translation units and tests did compile.

## Controller compatibility boundary

GLFW supplies common standardized state and generic raw joystick input. Actual mappings depend on the mapping database and platform. This package does **not** claim native Steam Input, XInput/Raw Input, Android NDK input, DualSense adaptive triggers, HD haptics, touchpad, sensors, speaker, microphone, RGB/player LEDs, battery state or USB/Bluetooth transport reporting. Those require additional native APIs/SDKs and device-specific validation.
