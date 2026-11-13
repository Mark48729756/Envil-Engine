# M51 — Split-sum IBL lighting integration groundwork

- Added irradiance-cubemap diffuse lighting, GGX-prefiltered environment reflection, and BRDF-LUT split-sum evaluation to both lighting shader variants.
- Added safe opt-in gates: OpenGL uses `uIBLReady` (default uniform value is zero); the RHI shader uses specialization constant `IBL_ENABLED` (default false).
- Kept the existing analytic ambient fallback so unconfigured renderers do not sample unbound environment resources.
- The IBL compute shader assets from M48 remain the preprocessing stages.
- Runtime integration is **not complete**: the renderer still needs HDR environment loading, cubemap/LUT allocation, compute dispatch, mip generation, descriptor/uniform binding, and resource lifetime management. Do not enable the gate before those steps are implemented.
