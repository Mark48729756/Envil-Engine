# M58 — Cascade controls and regression coverage

- Added editor controls for shadow distance and logarithmic/uniform split blending; relabeled the legacy radius control as RHI-specific.
- Added `ShadowCascadeContractTests` to verify cascade math, array-layer rendering, depth-view setup, blocker search and PCSS resolve contracts.
- Updated the earlier photorealism source contract to accept the new GL PCSS path while still checking the RHI's existing Poisson PCF path.
- No GLSL compiler or live GPU was available in this environment; tests validate source contracts, not shader compilation or rendered image quality.
