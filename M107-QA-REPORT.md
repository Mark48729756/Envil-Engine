# M107 QA report

## Executed

- Configured the dependency-free M103-M105/controller test build with CMake.
- Built `anvil_m103_m105_tests` and `anvil_controller_tests` with C++20.
- Ran CTest: **2/2 passed**.
- Controller test coverage includes SDL-style mapping-file ingestion, GLFW mapping dispatch, GUID vendor/product extraction, manufacturer lookup, hat-to-D-pad mapping, active input-device switching, axis deadzone/sensitivity/inversion, action bindings, rumble dispatch, profile save/load, and GUID mismatch rejection.

## Not executed / limitations

- Steamworks SDK headers/library and a running Steam client were unavailable; optional Steam code was not compiled against the real SDK or runtime-tested.
- No physical controllers were attached. Linux evdev permissions and real force-feedback behavior remain device/runtime dependent.
- Windows XInput and Android NDK branches require their respective native toolchains and hardware tests.
- Full engine/editor build was not run because the environment's graphics dependencies are not provisioned for a complete build.
- Compiled the optional Steam Input translation unit with `-Wall -Wextra -Werror` against a temporary local interface-shape stub to catch conditional-branch C++ errors. This is a syntax check only, not a substitute for compiling against the actual Steamworks SDK.
- The optional Steam branch syntax check includes `Init(bool)`, digital/analog action queries, motion-data reads, standard vibration and extended trigger-vibration calls using a local API-shape stub. No actual Steamworks binary was linked.
