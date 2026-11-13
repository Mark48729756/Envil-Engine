# M64 — World partition planning

- Added a CPU-side 3D grid partition planner with configurable cell size, load radius and larger unload radius.
- Updates report newly required cells and retain nearby resident cells to reduce boundary thrashing.
- The planner does not load scene files itself; requests still need to be passed to `AsyncAssetStreamer`, and completed data must be committed on the main/render thread.
