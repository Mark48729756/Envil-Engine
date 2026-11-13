# M84–M100 QA report

## Implemented foundations

- **M84:** Terrain Editor: heightfield sculpting, flattening and bilinear sampling CPU reference tools.
- **M85:** Node Editor: graph nodes, directed links and cycle prevention.
- **M86:** Asset Pipeline: extension classification, import options and asset metadata validation. No third-party FBX/glTF decoder is bundled.
- **M87:** Material Editor: editable scalar parameters with range clamping and material layer definitions.
- **M88:** Animation Editor: sorted float keyframes, replacement at duplicate times and linear sampling.
- **M89:** Hardware Ray Tracing foundation: CPU-side geometry/instance scene model and validation. DXR/Vulkan acceleration structures and GPU dispatch are not implemented.
- **M90:** Advanced GI: CPU-side irradiance probe volume storage and lookup foundation; no runtime probe capture or GPU integration.
- **M91:** Cinematic Rendering: camera/post settings validation and circle-of-confusion reference calculation.
- **M92:** Render Graph: pass dependency compilation and deterministic topological execution order.
- **M93:** GPU Profiler foundation: bounded timing history, average and percentile statistics; hardware timestamp queries remain backend work.
- **M94:** Multithreaded Rendering foundation: task dependency graph with cycle-safe topological ordering; worker pool and command recording integration remain future work.
- **M95:** Vehicle System: longitudinal force integration, braking, drag and steering input model; wheel suspension/contacts are not yet integrated.
- **M96:** AI System: four-neighbour grid A* pathfinding and obstacle map.
- **M97:** Gameplay Framework: inventory, persistent flags and in-memory save snapshots; disk serialization and mission scripting remain future work.
- **M98:** Build Pipeline: build manifest and safe relative artifact destination validation; platform packaging/signing is not implemented.
- **M99:** Multi-API Integration: runtime capability records and deterministic API preference selection; this does not add missing backend implementations.
- **M100:** Release SDK: package manifest validation for headers, runtime and editor; final commercial release requires platform builds, legal review, shader compilation and GPU QA.

## Validation

- 17 focused C++20 test executables were added to the dependency-free `ANVIL_ASSET_TESTS_ONLY` path.
- Tests cover terrain editing math, graph cycle rejection, asset type classification, material parameter clamping, keyframe interpolation, ray-tracing scene metadata validation, GI probe storage, cinematic settings, render-graph ordering, profiler statistics, task dependency ordering, vehicle force integration, A* navigation, gameplay snapshots, build path validation, API selection and SDK manifest validation.
- These tests do **not** prove integration with the renderer, GPU, audio device, OS packaging tools or commercial deployment.
- DXR/Vulkan RT, actual glTF/FBX decoding, GPU GI capture, hardware timestamp queries, multithreaded command recording, wheel suspension, disk save serialization and platform installer/signing remain incomplete.
