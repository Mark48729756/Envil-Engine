# M100

Release SDK: package manifest validation for headers, runtime and editor; final commercial release requires platform builds, legal review, shader compilation and GPU QA.

## Fix: black sphere artifacts + volumetric cloud rework
- lighting.frag (GL + RHI): gMaterial was read as vec2 but .z (AO) was used -> undefined value gave black speckles. Now vec3, plus NaN/Inf guard on lit output.
- gbuffer.frag: NaN guard for degenerate tangent frames with normal maps.
- Clouds: 24 jittered steps (IGN) with capped distance, 3-octave base + erosion detail only inside cloud, cheap 3-tap light march, Henyey-Greenstein phase, height-based ambient, horizon fade. Removes stripe/wave look, adds 3D volume, lower cost.

## M101: photoreal clouds + merge of ANVIL-shaders-fix into M100-AUDIT-FIXED
- Shape: Perlin-fbm + cellular (Worley-like) billows, large-scale coverage map, cumulus height gradient (flat base, rounded top), erosion detail with distance LOD.
- Lighting: 4-tap exponential light march, 3-octave multi-scattering approximation, dual-lobe Henyey-Greenstein (silver lining + back scatter), powder effect, height/occlusion-aware sky ambient, aerial perspective toward horizon haze.
- Performance: adaptive step length (path-based), 2x empty-space skipping with a conservative bound, early-out at 3% transmittance, erosion only near camera, cheap base-only light samples, IGN jitter (no banding).
- GL and RHI lighting.frag share identical cloud code; contract identifiers (renderVolumetricClouds, cloudDensity, CLOUD_STEPS) kept.
- RHI note: uCloudTime is a const 0.0 in rhi/lighting.frag, so clouds are static there until a time value is added to the Frame UBO.
