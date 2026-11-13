# ANVIL ENGINE — M14–M19

This increment prioritizes a practical 2015-era real-time rendering feature set and a first deterministic CPU physics layer.

## M14 — Physics foundation
- Added `anvil::physics::PhysicsWorld` with fixed-step accumulation, bounded substeps, gravity, body creation/destruction and stable numeric handles.

## M15 — Rigid-body contacts
- Added AABB-vs-AABB contact generation, minimum-penetration-axis resolution and inverse-mass positional correction.

## M16 — Material response
- Added restitution impulses and Coulomb-style friction impulses with clamped coefficients.

## M17 — Screen-space reflections
- Added bounded screen-space reflection ray marching, roughness fade and edge confidence fade. It is intentionally approximate and cannot see off-screen geometry.

## M18 — Atmosphere
- Added optional distance fog with configurable color and density.

## M19 — Image quality
- Added optional FXAA after HDR bloom composition and ACES tonemapping.

## Verified scope and limitations
- OpenGL 4.5 remains the only implemented graphics API. Vulkan and Direct3D 12 are not available in this increment.
- Physics is a prototype for axis-aligned boxes. It does not yet implement angular dynamics, continuous collision detection, joints, mesh collision, vehicles or character controllers.
- SSR is not ray tracing and should be paired with environment lighting/probes in a later increment.
- Full graphics runtime validation requires the external GLFW/GLM/GLAD FetchContent dependencies and an OpenGL 4.5 driver.
