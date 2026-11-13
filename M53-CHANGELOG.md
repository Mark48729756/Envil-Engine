# M53 — Bounded GPU asset cache

- Added byte estimates to RHI mesh and albedo-texture cache entries.
- Added a configurable `RenderSettings::gpuCacheBudgetBytes` budget (default 512 MiB, with a 16 MiB safety floor).
- Replaced fixed-age-only pruning with least-recently-used eviction under budget pressure and periodic stale-entry sweeps.
- Current-frame resources remain pinned by `shared_ptr` references in prepared draw records; evicting a cache entry cannot invalidate a draw already recorded for that frame.
- Exposed cache byte and entry counts through `RenderStats` for diagnostics.
- Texture byte accounting includes an approximate full mip-chain factor (4/3); backend allocation overhead is not included.
