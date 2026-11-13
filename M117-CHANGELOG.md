# ANVIL Engine M117 — glTF/GLB animation importer (stage A2a)

## Added
- `engine/include/anvil/GltfImport.h` (header-only, no third-party dependencies, includes its own JSON parser):
  - `importGltfFile(path)` / `importGltfFromMemory(data,size,baseDir)` for `.gltf` (external `.bin`, `data:` URIs) and `.glb`.
  - Skeleton from skin 0: joints may be listed in any order; bones are re-ordered so every parent precedes its children (required by `anvil::Skeleton`). Bind pose from node TRS or matrix.
  - Inverse bind matrices read from the file and remapped to bone order (falls back to the bind-pose inverse).
  - Animation clips: translation / rotation / scale channels, LINEAR, STEP and CUBICSPLINE.
    STEP is kept as a hard step (hold key inserted just before the next key); CUBICSPLINE is baked to 8 samples per segment so the linear runtime `Animator` reproduces the curve.
    Channels on the same bone are merged into one track on the union of their key times.
  - Skinned mesh data (`SkinnedMeshData` / `SkinnedVertex`): position, normal, uv, 4 joint indices already remapped to bone indices, 4 normalized weights.
  - `transformFromMatrix(AnimMat4)` — TRS decomposition (also used for glTF `matrix` nodes).
- Robustness: bounds-checked accessors (strides, normalized integers), buffer-uri path-traversal rejection (absolute paths, drive letters and `..` are refused), JSON depth limit, 512 MB file cap, GLB chunk validation.
- `tests/GltfImportTests.cpp`: GLB-from-memory and `.gltf`+`.bin` from disk, parent reordering, IBM remap, joint remap, linear/step/cubic sampling, end-to-end skinning palette, traversal rejection, truncated/garbage input, base64, matrix round trip.
  Also run under AddressSanitizer + UBSan and mutation-fuzzed (6000 corrupted GLBs, no crashes).

## Warnings reported instead of failing
Several skins (only skin 0 is used), non-joint nodes between joints, root joint under a non-identity node transform, morph-target weight channels, channels targeting nodes outside the skeleton, non-skinned or non-triangle primitives.

## Not supported yet
Sparse accessors, morph targets, multiple skins, FBX (glTF only for now).

## Animation roadmap
| Stage | Content | Status |
|---|---|---|
| A1 | Pose -> skinning pipeline, events, root motion | done (M116) |
| A2a | glTF/GLB skeleton, clip and skinned-mesh importer | done (M117) |
| A2b | GPU skinning wired into Renderer / RhiRenderer (skinned vertex format, bone palette buffer, skinned draw path for OpenGL/Vulkan/D3D12) | next |
| A3 | Animation graph v2: parameters, conditions, state machine, 1D/2D blend trees, layers, bone masks, additive, sync markers | planned |
| A4 | IK: FABRIK/CCD, foot IK with ground raycast, look-at/aim, hand IK | planned |
| A5 | Motion matching: feature database, pose search, trajectory prediction | planned |
| A6 | Physics blending: active ragdoll, spring/secondary bones, cloth hooks | planned |
| A7 | Compression (curve fitting, quantization), retargeting, facial blendshapes, FBX import | planned |
| A8 | Scale: animation LOD, multithreaded/GPU evaluation, crowd instancing, editor tooling | planned |
