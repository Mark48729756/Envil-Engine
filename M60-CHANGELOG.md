# M60 — Distance-based LOD selection

- Added backend-agnostic `LodSelector` in `engine/include/anvil/WorldOptimization.h`.
- Supports configurable distance thresholds, arbitrary LOD IDs, input sanitization and hysteresis to reduce boundary flicker.
- The selector is a reusable CPU-side primitive; model import, per-entity LOD asset groups and draw submission are not wired into the editor yet.
