# ANVIL Engine M46 — Rendering quality + visibility optimization

## Rendering optimization
- Added conservative local-space bounding spheres to every `Mesh`, computed once when the mesh is created/imported.
- Added a shared frustum implementation that extracts six planes from GLM view-projection matrices and supports both OpenGL `[-1, 1]` and Vulkan/D3D `[0, 1]` clip-depth conventions.
- Added conservative world-space sphere transformation. The scale bound uses `sqrt(||A||1 * ||A||inf)`, so non-uniform scale and shear do not shrink bounds and incorrectly hide geometry.
- OpenGL path now culls camera-invisible meshes from the G-buffer and shadow-map-invisible meshes from the sun shadow pass.
- RHI path applies the same camera and light-frustum tests before GPU resource preparation and skips albedo texture upload for shadow-only objects.
- Renderer stats expose submitted, camera-visible, camera-culled and shadow-visible object counts. The editor displays the counts.
- Invalid/non-finite bounds fail open (remain visible) to avoid disappearing geometry due to bad asset data.

## Image quality
- Replaced the flat background gradient with a procedural sky gradient: softer horizon transition, low-angle haze, solar disc and broad solar glow driven by the sun radiance.
- Added roughness/view-dependent specular ambient occlusion to reduce bright specular fill leaking through creases and contact areas.
- Kept GL and RHI lighting implementations aligned; a shader contract test checks that both retain the new sky and specular-AO features.
- Existing sRGB albedo decoding, GGX/Cook-Torrance direct lighting, HDR/ACES, bloom, SSAO, SSR and PCF shadows remain in place.

## Tests
- Added `anvil_frustum_culling_tests` for OpenGL and zero-to-one clip-depth plane extraction, boundary intersections, shear-safe sphere scaling and invalid-bound fail-open behavior.
- Added `anvil_rendering_shader_contract_tests` to prevent GL/RHI lighting shader drift.
- Dependency-free QA build: 7/7 CTest tests passed.

## Limitations / not claimed as complete
- Full engine configure/build could not run because this environment could not resolve GitHub to fetch GLFW/GLM/GLAD/ImGui dependencies.
- The new GLM frustum unit test is registered for the full build but could not be compiled on this host for the same dependency-fetch reason.
- GLSL compiler and real GPU validation were unavailable here; shader source contract tests passed, but runtime shader compilation/render comparison remains required on a graphics workstation.
- This milestone is not yet full photorealism: environment-cubemap IBL + prefiltered specular maps + BRDF LUT, tangent-space normal/roughness/metallic maps, TAA, cascaded shadows, and ray-traced GI remain future work.
- Frustum culling is not occlusion culling; objects inside the camera frustum can still be fully hidden behind other geometry.
