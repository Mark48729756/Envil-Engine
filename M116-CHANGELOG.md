# ANVIL Engine M116 — Animation Core v2 (stage A1)

## Added
- `engine/include/anvil/AnimationRuntime.h` (header-only, depends only on `Animation.h`):
  - Quaternion helpers: `quatMul`, `quatConjugate`, `quatRotate`, `quatFromAxisAngle`.
  - `AnimMat4` (column-major, GLSL-compatible), `matFromTransform` (TRS incl. non-uniform scale), `inverseAffine`, `transformPoint`.
  - Skeleton -> skinning: `findBoneIndex`, `computeModelPose`, `computeInverseBindPose`, `buildSkinningPalette`.
    The palette is `model * inverseBind` per bone and can be uploaded as-is to the `BonePalette` buffer of `shaders/rhi/skinning.vert`.
  - Animation events (notifies): `collectEvents` — fires events in (prev, curr], correct across any number of loop wraps, chronological order, `includeStart` for the first frame.
  - Root motion: `extractRootMotion` / `composeRootMotion` — delta between two playback times, loop-safe, translation reported in the root's local frame at interval start.
- `AnimationClip::events` (`AnimationEvent{time,name}`), additive; existing code and tests are unaffected.
- `tests/AnimationRuntimeTests.cpp`, registered in CMake next to the other animation tests.

## Not changed
Existing `Animator`, `AnimationGraph`, `Ragdoll`, renderer and shaders are untouched.

## Animation roadmap (one stage at a time)
| Stage | Content | Status |
|---|---|---|
| A1 | Pose -> skinning pipeline, events, root motion | done (this milestone) |
| A2 | glTF/FBX skeleton + clip import, GPU skinning wired into Renderer/RhiRenderer | next |
| A3 | Animation graph v2: parameters, conditions, state machine, 1D/2D blend trees, layers, bone masks, additive, sync markers | planned |
| A4 | IK: FABRIK/CCD, foot IK with ground raycast, look-at/aim, hand IK | planned |
| A5 | Motion matching: feature database, pose search, trajectory prediction | planned |
| A6 | Physics blending: active ragdoll, spring/secondary bones, cloth hooks | planned |
| A7 | Compression (curve fitting, quantization), retargeting, facial blendshapes | planned |
| A8 | Scale: animation LOD, multithreaded/GPU evaluation, crowd instancing, editor tooling | planned |
