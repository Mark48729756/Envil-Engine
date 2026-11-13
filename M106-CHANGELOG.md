# M106 — Native Controller Backends

- Added dynamically loaded Windows XInput fallback with standard button/axis/trigger mapping and dual-motor rumble.
- Added Linux evdev discovery, VID/PID/name extraction, USB/Bluetooth transport inference from sysfs, standard gamepad mapping and force-feedback rumble when FF_RUMBLE is advertised.
- Added Android NDK event ingestion API for key, D-pad, stick and trigger input, plus NDK device enumeration.
- Added collision-free synthetic ID namespaces for native backends and avoided common GLFW/evdev duplicates.
- Upgraded controller profile output to format v2 while retaining v1 profile loading compatibility.
- Added Android `android` library linkage in CMake.
- Kept unsupported device-specific features unadvertised; Steam Input and vendor-specific DualSense/Nintendo features remain pending their official SDKs/APIs.

## Verification

- `ANVIL_M103_M105_TESTS_ONLY=ON` configure/build: passed.
- CTest: 2/2 passed.
- Linux controller translation unit: compiled with `-Wall -Wextra -Werror`.
- Address/undefined sanitizer and physical-device tests: not run in this revision.
- Windows XInput and Android NDK branches: not compiled in this Linux environment.
