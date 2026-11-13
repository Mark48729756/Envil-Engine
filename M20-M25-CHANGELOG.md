# ANVIL ENGINE — M20–M25

## M20 — Jolt Physics integration path
- Added an optional Jolt Physics 5.2 adapter (`JoltPhysicsWorld`) with box rigid bodies, static/dynamic bodies, gravity, friction, restitution and fixed-step updates.
- Existing dependency-free `PhysicsWorld` remains available as a small prototype and fallback.
- Enable with `-DANVIL_WITH_JOLT=ON`; CMake fetches Jolt and compiles the adapter.

## M21 — RHI backend selection
- Expanded backend identity to OpenGL 4.5, Vulkan 1.2 and Direct3D 12.
- Added backend naming, availability query and a central device factory.
- The factory explicitly rejects Vulkan and D3D12 until complete native resource, pipeline, command submission and presentation implementations are merged. These two APIs are NOT claimed as working yet.

## M22 — Better point-light falloff
- Added configurable light radius and smooth finite-range attenuation on top of inverse-square falloff.
- Added scalar uniform-array upload support.

## M23 — Rendering stability
- Kept the HDR -> bloom -> ACES -> FXAA order and improved light-range behavior to avoid abrupt cutoffs.

## M24 — Physics migration compatibility
- Kept the existing PhysicsWorld API intact so sample projects can continue to build while Jolt is opt-in.

## M25 — Documentation and validation
- Documented backend status and exact Jolt build switch.
- Dependency-free asset tests remain independently buildable.

## Honest implementation status
- OpenGL 4.5: implemented.
- Jolt: adapter source and opt-in CMake integration added; not built in this offline validation because fetching Jolt requires network access.
- Vulkan 1.2: backend identity/factory slot only; no functioning renderer backend yet.
- Direct3D 12: backend identity/factory slot only; no functioning renderer backend yet.
- Renderer improvements in this increment are point-light attenuation, not a complete new renderer or a claim of 2015 AAA visual parity.
