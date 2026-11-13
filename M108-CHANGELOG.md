# M108 — Controller remapping and input prompts

- Added per-device physical-to-logical button remapping for all existing controller backends.
- Added explicit unbind support (`logicalButton = -1`), mapping lookup, and per-device reset.
- Added controller glyph styles for Generic, Xbox, PlayStation, and Nintendo layouts.
- Added best-effort glyph-style inference from the reported controller name and `buttonPrompt()` for UI labels.
- Added controller profile format v3 with `BUTTON_MAP` records; v1 and v2 profiles remain readable.
- Applied remapping to the logical `ControllerState::buttons` array while preserving `physicalButtons` as raw input for diagnostics and rebinding UI.
- Gamepad activity tracking now also observes native and Steam Input state returned through the common state API.
- Added regression tests for remapping, raw-state preservation, prompts, glyph inference, and profile round-tripping.

## API sketch

```cpp
controllers.setButtonMapping(deviceId, /*physical*/ 1, /*logical*/ 0);
controllers.setButtonMapping(deviceId, /*physical*/ 2, /*logical*/ -1); // unbind
const auto label = controllers.buttonPrompt(deviceId, 0);
controllers.resetButtonMappings(deviceId);
```

The engine exposes labels and normalized state; the host/editor UI remains responsible for drawing the actual glyph artwork and for capturing a user's next physical input during a rebinding screen.
