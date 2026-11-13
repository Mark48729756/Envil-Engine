# M50 — High-quality shadow filtering

- Replaced the 3x3 regular shadow kernel in the OpenGL and RHI lighting shaders with a 16-tap Poisson-disk PCF filter.
- Rotates the sampling kernel per pixel to reduce visible grid/banding patterns.
- Retains the existing normal offset, slope-dependent bias, shadow enable gate, and depth bounds checks.
- This is improved filtering for the existing single shadow map, **not Cascaded Shadow Maps**. Cascade split computation, multiple shadow-map rendering, atlas allocation, and renderer descriptor plumbing remain future work.
