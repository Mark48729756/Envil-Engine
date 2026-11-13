# M106 QA Report — Native Controller Backends

## Source changes

- `engine/include/anvil/Controller.h`: pimpl lifetime, non-copyable ownership and Android event-forwarding API.
- `engine/src/Controller.cpp`: Windows XInput fallback; Linux evdev discovery, polling, VID/PID, USB/Bluetooth classification and FF_RUMBLE; Android NDK device enumeration and key/axis ingestion.
- `engine/CMakeLists.txt`: Android `android` native library linkage.
- `README.md`, `M106-CHANGELOG.md`, `M106-NATIVE-CONTROLLER-BACKENDS.md`: integration and support boundaries.

## Executed checks

| Check | Result |
|---|---|
| CMake configure, `ANVIL_M103_M105_TESTS_ONLY=ON` | Passed |
| C++20 build | Passed |
| CTest | 2/2 passed |
| Linux controller TU with `-Wall -Wextra -Werror` | Passed |
| AddressSanitizer + UndefinedBehaviorSanitizer build | Passed |
| Sanitized CTest | 2/2 passed |
| Windows XInput branch syntax check against minimal Win32/XInput declarations | Passed; not a Windows SDK build |
| Android NDK branch syntax check against minimal NDK declarations | Passed; not an Android NDK build |
| ZIP integrity | Checked after packaging |

## Not tested / not claimed

- No physical gamepad was connected to the build environment; USB, Bluetooth, hotplug, and motor output were not hardware-tested.
- The full editor/engine build was not run because this environment cannot fetch the project's GLFW/GLM/GLAD/ImGui dependencies from GitHub.
- Windows and Android branches were syntax-checked with local declaration shims, not compiled with their official SDK/toolchains.
- Steam Input is not implemented in this milestone. It needs the Steamworks SDK and the host application's Steam API lifecycle.
- DualSense adaptive triggers, HD haptics, touchpad, gyro, battery reporting, controller speaker/microphone and Nintendo-specific extensions are not implemented by these generic backends.
- Android rumble is not exposed by the portable NDK input API used here; `setRumble` returns false for Android synthetic IDs.

## Runtime integration notes

- GLFW input remains the preferred standard-gamepad path where it already recognizes the controller.
- Linux evdev is used as a fallback for devices not already surfaced by GLFW. Access to `/dev/input/event*` depends on the host's device permissions.
- On Android, the app must forward its existing NativeActivity/GameActivity `AInputEvent` objects to `ControllerSystem::handleAndroidInputEvent`; ANVIL does not take over the activity's event queue.
