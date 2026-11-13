# ANVIL Engine M109 — Controller Configuration Workflow

## Added
- One-shot controller input capture API for editor binding workflows.
- Capture reports the first newly pressed physical button, or the first axis that crosses a configurable activation threshold.
- Capture snapshots current button state on start, preventing a button held before capture from being incorrectly recorded.
- Capture can be cancelled, queried for pending state, and automatically ends when its controller disconnects.
- Physical input is preferred for capture so remapped logical buttons do not obscure the source control; logical state is used as a fallback for backends that do not expose physical counts.

## Compatibility
- No change to the controller profile format; v1-v3 remain supported.
- Existing controller mappings, action bindings, axis settings, and runtime backends are unchanged.

## Editor integration
The Controller Debugger now exposes per-logical-button Rebind actions, live capture feedback, cancel/reset controls, per-axis deadzone/sensitivity/inversion sliders, and GUID-scoped profile save/load under `ControllerProfiles/`. Capturing a button replaces any previous physical binding for that logical slot. Axis capture is detected by movement away from the axis baseline, so triggers resting at -1 do not spuriously bind.

## Engine API
The editor can call `beginInputCapture(deviceId)` when the user selects a binding row, poll `pollInputCapture()` from its update loop, then apply the returned `Button` index with `setButtonMapping` or use the `Axis` index with `bindAxis`. Call `cancelInputCapture()` when the dialog closes.
