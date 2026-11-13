# M55 — Temporal/cache diagnostics and regression contracts

- Added a portable source-contract test for TAA history/depth rejection and bounded RHI cache behavior.
- Registered the new test in both dependency-free asset-test and full engine test configurations.
- Added editor controls and explicit UI guidance for the current temporal reprojection limitation.
- Kept the RHI cache diagnostics in the shared `RenderStats` structure so backend-specific editor panels can expose them without duplicating accounting.
