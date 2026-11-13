# M57 — PCSS soft sun shadows

- Added a non-comparison texture view over the same immutable depth-array storage for blocker search.
- Added a 16-sample blocker search and average blocker depth estimate.
- Derives a bounded penumbra/filter radius from receiver-to-blocker depth and applies rotated 16-tap comparison PCF.
- Uses per-cascade texel-world scale for normal offset and preserves a hard-shadow fallback when no blockers are found.
- The raw depth view uses a white border to avoid false blockers outside the shadow map.
