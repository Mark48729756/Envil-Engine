# M103–M105 QA report — continuation build

## Test configuration

```sh
cmake -S . -B build-m103 -DANVIL_M103_M105_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-m103 -j2
ctest --test-dir build-m103 --output-on-failure
```

## Result

- `anvil_m103_m105_tests`: passed. Exercises PE x64 metadata and DLL import parsing, Direct3D API hinting, a deflated APK binary AndroidManifest.xml (package, permission, min/target SDK), ABI inventory, project creation/validation/multi-target configs/recent list, project asset inventory and migration copy/skip behavior.
- `anvil_controller_tests`: passed. Uses a deterministic GLFW API test double to exercise enumeration, raw physical input snapshots, deadzone/sensitivity/inversion, action/axis mapping, rumble forwarding and GUID-bound profile save/load.
- Total: **2/2 tests passed**.
- AddressSanitizer + UndefinedBehaviorSanitizer build: **2/2 tests passed**, no sanitizer findings.
- `-Wall -Wextra -Wpedantic` build: completed without compiler warnings.

## Full engine build status

A normal full-engine configure was attempted with `BUILD_TESTING=ON`. CMake stopped at `FetchContent_MakeAvailable(glfw)` because the environment could not resolve `github.com` (`Could not resolve host: github.com`). This is an external dependency-fetch failure, not a successful full-engine build. The editor UI translation unit and full engine link are therefore not verified in this environment.

## Hardware / SDK validation

No physical controllers, Windows XInput/Raw Input environment, Android device, Steam Input runtime, or closed-console SDKs were available. Controller tests use a local GLFW test double and do not establish real-device compatibility. Per-platform INI files are project metadata, not build toolchains.
