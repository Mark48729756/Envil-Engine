# M109 QA Report

- Added deterministic unit coverage for button capture rising-edge behavior.
- Added deterministic unit coverage for axis capture threshold and signed axis value.
- Added coverage for invalid device IDs and capture pending state.
- Existing controller mapping, profile persistence, action, rumble, and device-disconnect tests remain in the same suite.
- Controller Debugger source now includes Rebind, cancel, reset, per-axis deadzone/sensitivity/inversion controls, and GUID-scoped profile save/load controls.
- Axis capture compares movement against the starting baseline to avoid false activation from resting trigger values.
- Full editor/renderer build and physical hardware tests are not claimed by this report; the GUI integration is source-reviewed but not compiled in this dependency-limited environment.
