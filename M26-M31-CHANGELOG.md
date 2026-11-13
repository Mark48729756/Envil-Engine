# ANVIL ENGINE — M26–M31

## M26 — Scene / physics bridge
- Added `Scene::attachPhysicsBody`, `detachPhysicsBody` and `stepPhysics`.
- Root scene entities can own physics bodies; dynamic positions are synchronized back to scene transforms.
- Removing an entity releases its attached physics body. Parented entities are rejected until rigid-body transform semantics for hierarchies are defined.

## M27 — Physics stability controls
- Added frame-rate-independent exponential linear damping and per-body gravity scale to the CPU physics prototype.
- Sanitized non-finite mass, restitution, friction, damping, gravity scale, position and velocity inputs.
- Gravity setters now reject non-finite values; fixed-step validation remains strict.

## M28 — Jolt initialization safety
- Made process-wide Jolt factory/type registration thread-safe with `std::call_once`.
- Jolt remains opt-in (`ANVIL_WITH_JOLT=ON`) and requires fetching/building the external Jolt dependency.

## M29 — PBR ambient energy handling
- Replaced the previous flat ambient multiplier with a roughness-aware Fresnel approximation and separate diffuse/specular ambient terms.
- SSAO now modulates the diffuse ambient contribution rather than suppressing the specular fill.
- This remains a hemispherical approximation, not IBL; no environment cubemap convolution or BRDF LUT is claimed.

## M30 — Physics regression coverage
- Added `PhysicsWorldTests.cpp` covering static/dynamic bodies, floor contact, finite simulation output, fixed-step validation and body removal.
- The test is part of the normal `BUILD_TESTING` configuration and requires the normal GLM dependency.

## M31 — Verification and backend status
- Dependency-free configuration/build/CTest: 2/2 passed (Asset Database and RHI selection).
- Full engine build and the new GLM-dependent physics test were not run because this environment has no GLM checkout and external dependency downloads are unavailable.
- OpenGL 4.5 remains the only implemented graphics backend. Vulkan 1.2 and Direct3D 12 remain unavailable and are not represented as implemented.
