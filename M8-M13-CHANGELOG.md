# ANVIL ENGINE — M8 to M13

This delivery groups six incremental milestones on top of M7.

## M8 — Project Asset Database
- Recursive project scan with permission-error tolerance and symlink avoidance.
- Stable non-zero 64-bit FNV-1a asset IDs derived from normalized project-relative paths.
- Asset classification for textures, models, materials, scenes and shaders.
- Deterministic index ordering and lookup by ID or relative path.

## M9 — Texture Import Foundation
- Added RAII `Texture2D` GPU resource and mipmapped OpenGL upload.
- Added PPM P3/P6 decoding with dimension, channel-range and truncation checks.
- sRGB internal format is used for base-color images; callers can request linear RGB.
- Current decoder is deliberately small and only supports PPM. PNG/JPEG decoding is not included yet.

## M10 — Textured PBR Base Color
- Material can reference a base-color texture while retaining its scalar albedo tint.
- G-buffer samples the texture in UV space and multiplies it by the material tint.
- Renderer binds the texture only for materials that provide one; untextured materials retain their previous behavior.

## M11 — OBJ Model Import
- Added OBJ vertex/UV/normal parsing, negative indices, polygon fan triangulation and vertex deduplication.
- Missing normals are generated from triangle geometry.
- Malformed indices, truncated faces and files without renderable faces return errors instead of crashing.
- Scene entities can now reference an OBJ model path and retain a shared GPU mesh.

## M12 — Scene + Editor Integration
- Scene format bumped to v4; v1-v3 remain accepted.
- Texture and OBJ paths are serialized as project asset references.
- Fixed entity-array bracket matching in the scene reader so vector arrays inside entities do not prematurely terminate the array parse.
- Inspector exposes OBJ path and base-color PPM path; project browser shows asset type hints and can refresh the asset index.
- Hierarchy-aware focus, viewport selection, selection outline and move gizmo now use world transforms. Translation gizmos convert world-space movement back into parent-local coordinates.

## M13 — Validation and Packaging
- Added a CTest target for asset database classification, indexing, stable IDs and lookup.
- Added `ANVIL_ASSET_TESTS_ONLY=ON` to configure/build/run this test without downloading GLFW or other graphics dependencies.
- Added small sample PPM and OBJ assets.
- Added this changelog and updated project documentation.

## Verification status
- `AssetDatabaseTests.cpp` compiled with GCC 14 in C++20 mode and passed.
- Dependency-free CMake test-only configuration built successfully; CTest reported 1/1 passing.
- ZIP archive integrity is checked before delivery.
- Full CMake configure/build could not complete in this environment because FetchContent could not resolve `github.com` while downloading GLFW. Consequently the GL-dependent translation units and runtime rendering were not compile- or runtime-verified here.

## Known limitations
- Texture decoding currently supports PPM P3/P6 only; PNG/JPEG and compressed GPU formats are future work.
- Model import currently supports OBJ only; glTF/GLB, material libraries and skeletal animation are not implemented.
- OBJ import currently creates one mesh and does not preserve OBJ material groups or MTL materials.
- Asset IDs are path-derived and will change when an asset is renamed or moved.
- Asset paths are project-relative by convention; the current editor is a lightweight path-entry workflow, not a full drag-and-drop importer.
- The existing renderer still uses OpenGL directly; the earlier RHI abstraction is not yet the active renderer backend.
