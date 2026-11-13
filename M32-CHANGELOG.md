# ANVIL ENGINE — M32: Jolt in Scene / ECS (stage 1 of 6)

Roadmap: **1. Jolt in Scene/ECS (this ZIP)** → 2. Vulkan renderer → 3. DirectX 12 renderer → 4. IBL + HDR → 5. Cascaded shadows + better reflections → 6. Physics/render optimization. Goal: ~2015 AAA-level graphics.

## What is new
- **`IPhysicsBackend`** (`physics/PhysicsBackend.h`): one interface for Scene, two implementations.
  - **Jolt** (`-DANVIL_WITH_JOLT=ON`): box / sphere / capsule, full 3D rotation + angular velocity, static / kinematic / dynamic, CCD for fast bodies, triggers, begin/end contact events, raycasts, forces/impulses, damping, gravity scale, sleeping.
  - **Simple** fallback: the old AABB solver behind the same interface (no rotation; sphere/capsule become boxes; no events).
- **Rigidbody component** (`Entity::physics`): authoring data on the Entity, so it is saved, duplicated and restored by the editor's Play/Stop snapshot. Auto-fit collider from Sphere/Cube/Plane primitive + entity scale, or manual box/sphere/capsule.
- **ECS runtime**: while physics runs, `Scene` owns an `ecs::Registry` with a `PhysicsBody` component per simulated entity (entity id == ECS id). `Registry::valid` is now O(1) (was a linear scan) and `createWithId` was added.
- **Scene API**: `startPhysics / stopPhysics / stepPhysics`, `addImpulse / addForce / setLinearVelocity / getLinearVelocity`, `teleportBody`, `raycast` (returns entity id, point, normal), `drainCollisions`, `setGravity`.
  - Parented bodies work: simulated in world space, written back to local space. Reparenting while running keeps the body in sync. Kinematic bodies follow the entity transform.
  - The M26 API (`attachPhysicsBody` / `detachPhysicsBody`) still compiles and works.
- **Editor**: Play starts physics, Stop destroys bodies and restores the snapshot. Inspector has a Rigidbody section and *Add Component → Rigidbody*. Demo scene now has physics on the ground, spheres, the cube and 6 tumbling crates.
- **Scene format v5**: Rigidbody fields (v1–v4 scenes still load, physics disabled). Loading into a running scene stops physics first.
- **Build**: `-DANVIL_PHYSICS_ONLY=ON` builds just the physics backends + tests (needs only glm, and Jolt if enabled — no window/GL).

## Bugs found and fixed in older code
1. **Scene Save/Load lost vectors**: `SceneSerializer` kept only X of every vec3 (position, scale, rotation, colors came back as `(x,0,0)`). Fixed in the field regex.
2. **Jolt crashed at startup**: the allocator was registered after members that allocate; now done in a first base class (`jolt::RegisterFirst`). Affects the old `JoltPhysicsWorld` too.
3. **Jolt CMake never worked**: `FetchContent` needed `SOURCE_SUBDIR Build` (Jolt's CMakeLists is in `Build/`).
4. **`JoltPhysicsWorld` bodies had zero inertia**: `MassAndInertiaProvided` was used with only a mass; now `CalculateInertia`.
5. **GCC/Clang build error** in `SceneSerializer.cpp` (`glm::vec3 d = {0}` is ill-formed outside MSVC).
6. Tests used `assert` (no-op in Release); new tests use a `CHECK` macro that always runs.

## Verification (Ubuntu, GCC, Release, Jolt 5.2.0, GLM 1.0.1, GLFW 3.4)
- Full project (engine, editor, sandbox, tests) compiles with and without Jolt.
- CTest 4/4 passed with Jolt and 4/4 without. New tests cover: box/sphere/capsule resting on a floor, tumbling rotation, rolling sphere angular velocity, raycasts, impulses, kinematic bodies, CCD vs a thin wall, trigger + contact events, invalid input (NaN, negative mass), Scene→ECS→backend sync incl. parented bodies, remove-while-running, reparent, legacy API, scene v5 round-trip, v4 compatibility, v6 rejection.
- **Not verified**: the editor and sandbox were compiled but never run (no display / OpenGL context in the build environment). The Rigidbody inspector and Play-mode behavior in the GUI are untested visually. MSVC was not tried.

## Known limits
- Euler angles on the Entity (`glm::eulerAngles`) are limited to ±90° pitch, so a body that tumbles through gimbal-lock orientations can show a flipped Euler readout (the physics itself uses quaternions and is unaffected).
- Triggers/events exist only on Jolt. Changes to a Rigidbody during Play apply on the next Play.
- Vehicles, character controller, joints and mesh/convex colliders are not part of this stage.
