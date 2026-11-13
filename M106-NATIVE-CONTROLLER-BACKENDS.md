# M106 — Native controller backends

## Implemented in this revision

### Windows — XInput fallback
- Dynamically loads `xinput1_4.dll`, then `xinput1_3.dll`, then `xinput9_1_0.dll`; no hard link to a specific XInput DLL is required.
- Polls all four XInput user slots.
- Maps standard face buttons, shoulders, menu/guide, stick clicks, D-pad, both sticks and analog triggers into the ANVIL standard state layout.
- Exposes dual-motor vibration through `XInputSetState`.
- XInput enumeration is used as a fallback when GLFW has not already exposed a standard gamepad, avoiding the common duplicate Xbox entry.

### Linux — evdev backend
- Scans `/dev/input/event*`, opens readable gamepad event nodes non-blocking and checks EV_KEY/EV_ABS capabilities before accepting a device.
- Reads device name, VID/PID, axis ranges, key state, stick axes, trigger axes and hat/D-pad state.
- Detects USB/Bluetooth transport from the sysfs device path when the kernel exposes it; otherwise reports Unknown.
- Detects FF_RUMBLE capability and sends real force-feedback effects with independently controlled strong/weak motor magnitudes.
- Exposes raw gamepad state through the same ControllerState layout. Device IDs use the 2000+ namespace to avoid collisions with GLFW IDs.
- If the same VID/PID device is already surfaced by GLFW, the evdev duplicate is suppressed.

### Android NDK — event-fed backend
- Adds `ControllerSystem::handleAndroidInputEvent(const AInputEvent*)` on Android builds.
- Maps Android gamepad keys, D-pad, joystick axes and analog triggers into the common ControllerState.
- Enumerates NDK input devices whose source includes GAMEPAD or JOYSTICK.
- The NativeActivity/GameActivity input queue must forward events to this method. ANVIL does not take ownership of the app's input queue.
- Links the Android `android` native library when `ANDROID` is enabled in CMake.

## Honest limitations

- Steam Input is not compiled in this revision. It requires the Steamworks SDK, Steam API lifecycle ownership by the host application, action-set/action-manifest configuration, and a Steam runtime. No Steam API is simulated.
- Android standard NDK input does not provide a portable rumble, battery, touchpad, gyro, adaptive-trigger or speaker API. Those require device-/vendor-specific services or a different Android integration.
- Windows XInput does not expose a reliable model string, transport, battery, VID/PID, gyro, touchpad or adaptive triggers. Those remain Unknown/unreported rather than guessed.
- Linux evdev device access depends on OS permissions (typically an input group/udev rule). Battery and vendor-specific sensors are not inferred from ordinary evdev events.
- This backend revision was compiled and exercised on Linux with the dependency-free controller test target. Physical USB/Bluetooth hardware was not attached to the build environment. Windows and Android branches require their respective SDK/toolchains and were not cross-compiled here.

## Build and integration

- Desktop backends are integrated into `engine/src/Controller.cpp`; no extra runtime package is required beyond the existing GLFW dependency.
- Android builds additionally link `android` in `engine/CMakeLists.txt`.
- Keep GLFW polling and ControllerSystem calls on the application's input/main thread. On Android, forward input events from the activity's existing queue callback.
