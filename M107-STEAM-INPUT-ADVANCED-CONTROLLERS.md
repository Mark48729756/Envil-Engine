# M107 — Steam Input and controller metadata

## Steam Input (optional)

Steam Input is an opt-in compile-time backend. The default build has no Steamworks dependency and continues to use GLFW plus the native Windows/Linux/Android backends.

Configure with:

```sh
cmake -S . -B build -DANVIL_WITH_STEAM_INPUT=ON \
  -DSTEAMWORKS_SDK_ROOT=/path/to/steamworks_sdk
cmake --build build
```

The SDK must contain `public/steam/steam_api.h` and a platform `steam_api` library in a supported redistributable/lib directory. The host application owns `SteamAPI_Init` / `SteamAPI_Shutdown`. ANVIL only initializes and shuts down the Steam Input subsystem; it does not initialize or shut down the global Steam API.

Example setup (after the host has successfully called `SteamAPI_Init`):

```cpp
anvil::ControllerSystem controllers(window);
controllers.bindSteamDigitalAction("Jump", 0);
controllers.bindSteamDigitalAction("Interact", 1);
controllers.bindSteamAnalogAction("Move", 0, 1);
std::string error;
if (!controllers.initializeSteamInput("examples/steam_input/actions.json",
                                      "InGameControls", &error)) {
    // Keep the regular GLFW/native controller path active.
    logWarning(error);
}

// In the normal input/update loop:
auto devices = controllers.connected();
for (const auto& device : devices) {
    auto state = controllers.state(device.id);
    if (state.connected && state.buttons[0]) { /* Jump */ }
}

// Before destroying ControllerSystem, or when leaving Steam Input mode:
controllers.shutdownSteamInput();
```

Steam Input action and action-set names may be supplied as the manifest identifiers (`Jump`, `Move`, `InGameControls`) or as their structural paths; ANVIL reduces structural paths to the identifiers Steamworks expects.
- `setTriggerRumble(id, left, right)` forwards to Steam Input extended vibration trigger channels where the controller and driver support them; unsupported hardware may ignore the request.
- `motionState(id)` reads Steam Input's fused orientation quaternion, acceleration and angular velocity for connected Steam Input devices. The first read can wake the controller IMU; early orientation samples may still be identity. `available` means the Steam Input query succeeded, not that every controller model contains an IMU.

The sample manifest is `examples/steam_input/actions.json`. Its action set is `InGameControls`, with digital actions `Jump` and `Interact`, and analog action `Move`.

## Controller metadata and input UX

- `ControllerInfo` now exposes best-effort `vendorId`, `productId`, and `manufacturer` for known USB vendor IDs. Unknown manufacturers remain empty; no device model is guessed.
- SDL GameControllerDB-style mapping files can be loaded with `loadGamepadMappings(path, &loaded, &error)`. Blank/comment lines are skipped; malformed records fail with a line number; GLFW decides whether each valid mapping is accepted.
- `ControllerState` exposes raw hat values and `physicalHatCount`; the first hat also contributes to the standard D-pad button indices.
- `activeInputDevice()` reports the last detected gamepad activity or keyboard/mouse activity explicitly reported by `notifyKeyboardMouseActivity()`. The application must call that method from its own keyboard/mouse callbacks; GLFW joystick polling alone cannot observe keyboard/mouse events.
- Controller profiles continue to preserve axis settings and action bindings. Steam manifest bindings are application configuration and are intentionally not silently written into per-device profiles.

## Current feature coverage

| Capability | Status |
|---|---|
| GLFW mappings, raw buttons/axes/hats | Implemented |
| Windows XInput state and dual-motor rumble | Implemented in M106 |
| Linux evdev state and force feedback | Implemented in M106; permissions required |
| Android NDK event-fed gamepad state | Implemented in M106; activity must forward events |
| Steam Input digital/analog actions, vibration, optional trigger rumble and motion-sensor query | Optional SDK backend |
| Manufacturer metadata | Best-effort known USB vendor IDs |
| Battery, touchpad gesture stream, adaptive trigger effects, speaker/microphone | Not exposed through ANVIL's current portable backends; Steam Input motion data is available through `motionState()` |

## Verification boundary

The default dependency-free controller and import test targets are built and run in CI-style environments. This environment does not include the Steamworks SDK or a Steam runtime, so the `ANVIL_WITH_STEAM_INPUT=ON` branch has not been compiled against Valve's actual headers or exercised with Steam. Windows and Android branches also still require their native SDK toolchains and hardware/runtime testing. No claim of physical-device validation is made.
