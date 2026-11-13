# ANVIL ENGINE — M43: RHI v3 — Sprint 1, module 2, steps 1-7

## Step 1 — API (RHI.h)
- Formats (appended): RG32F, RGBA32F, R16, RG16, RGBA16, R32UI, RGBA8Snorm, BC1..BC7 (+sRGB / snorm / BC6H).
- `TextureDimension` (Tex2D, Cube, Tex2DArray, CubeArray, Tex3D), `TextureDesc::dimension/layers/depth`, `TextureSubresourceData`,
  `Device::createTexture`, `Device::supportsFormat`, `Texture::dimension/levels/layers/depth`.
- `TextureViewDesc`, `CommandList::bindTextureView`, `ColorAttachment/DepthAttachment::layer/layerCount`.
- Compute surface: `ShaderDesc::compute*`, `ComputePipeline`, `createComputePipeline`, `setComputePipeline`, `dispatch`, `dispatchIndirect`,
  `bindStorageTexture` (set 2), `bindStorageBuffer` (set 3), `BufferUsage::Indirect`, `copyTexture`, `generateMipmapsRange`.
- New `DeviceCapabilities` fields. All new virtuals default to "unsupported": OpenGL and D3D12 compile unchanged.

## Step 2 — Vulkan textures (VulkanRHI.cpp)
- `createTexture`: 2D, cube (6 layers, square, cube-compatible image), 2D array, cube array, 3D volumes; per-subresource initial data
  (mip + layer, optional rowPitch, RGB8 expanded to RGBA8), one staging buffer + one copy per subresource.
- Only mip 0 supplied + levels > 1 -> mips are blit-generated for every layer / volume slice; supplied mips and BCn are used as given.
- Full set of new formats mapped; BC1-BC7 gated on `textureCompressionBC`, cube arrays on `imageCubeArray` (both enabled on the device when present).
- `supportsFormat()` queries optimal-tiling feature bits (sampled / colour / depth / storage).
- `Texture`: `view_` = 2D view of layer 0 (attachments, plain 2D sampling), `sampleView_` = full view of the texture type (CUBE / 2D_ARRAY / CUBE_ARRAY / 3D)
  over all mips and layers -> existing `bindTexture` samples a cubemap or array with no extra code.
- Barriers now span every array layer (`VK_REMAINING_ARRAY_LAYERS`), `generateMipmaps` handles layers and volumes.
- Capabilities filled: BC, cubeArrays, maxArrayLayers, maxCubeSize, maxTexture3DSize, compute limits.

## Step 3 — Vulkan views, layered rendering, copies (VulkanRHI.cpp)
- Layout tracking is now per subresource (mip x layer). It stays a single value while every subresource agrees, so whole-image
  transitions still cost one barrier; barriers are emitted only for the touched range.
- Render into a cube face / array slice / mip: `ColorAttachment` and `DepthAttachment` `mip`, `layer`, `layerCount`. Attachment views have exactly one mip
  (this also fixes colour targets that have more than one mip level). Render area follows the mip size.
- `layerCount > 1` (one pass, several layers via gl_Layer) needs `caps.shaderOutputLayer`; the Vulkan 1.2 feature is enabled when the GPU has it.
  Without it the call throws `UnsupportedByBackend`: render the faces one by one.
- `bindTextureView`: sampled view of any mip / layer range (cube face as 2D, array slice, mip chain tail, cube out of 6 layers of an array...).
  Cached per texture; `ViewType::Auto` picks the type. A texture can be sampled in a pass while a different mip/layer of it is rendered to;
  overlapping ranges still throw.
- `copyTexture` (mip + layer to mip + layer, same format and extent), `generateMipmapsRange` (per layer range, volumes included); `generateMipmaps` uses it.
  Textures need `TextureUsage::TransferSrc` / `TransferDst` for copies.

## Step 4 — Vulkan compute (VulkanRHI.cpp, SpirvReflect.h)
- Descriptor sets: set 2 = 8 storage images, set 3 = 8 storage buffers (compute stage). Sets 0/1 are now visible to compute too. One shared pipeline layout
  (sets 0..3, 128 B push constants for vertex | fragment | compute). Pools got storage image / buffer sizes. Fallback buffer also has STORAGE usage (unbound set-3 slots).
- `caps.compute` / `caps.storageImages` are on when the queue family has COMPUTE and the per-stage descriptor limits fit; otherwise the layout stays 2-set and compute calls throw `UnsupportedByBackend`.
- Device features enabled when present: `shaderStorageImageExtendedFormats`, `shaderStorageImageRead/WriteWithoutFormat`.
- `ShaderDesc::computeSpirv` -> `Shader` (either vertex+fragment or compute). `createComputePipeline`; `localSize()` is read from the SPIR-V `LocalSize` mode
  (LocalSizeId / spec constants report 1x1x1 + a log line); size is validated against device limits. `rhi::loadComputeShaderDesc`, `.comp` in AnvilShaders.cmake.
- `setComputePipeline`, `dispatch` (count limits checked, zero = no-op), `dispatchIndirect` (needs `BufferUsage::Indirect`).
- `bindStorageTexture(slot, tex, mip, baseLayer, layerCount)`: needs `TextureUsage::Storage` (now validated at creation: no depth / BCn / MSAA, format must support storage).
  Tex3D -> 3D view; layerCount 1 -> image2D; otherwise image2DArray. Moved to GENERAL per subresource in one batched barrier per dispatch (also GENERAL->GENERAL for write ordering).
  A subresource sampled and stored in the same dispatch throws.
- `bindStorageBuffer(slot, buf, offset, range)`: `Storage` / `Indirect` buffers, offset alignment + range limits checked; nullptr unbinds.
- After a dispatch with storage buffers a memory barrier covers compute, vertex/index/indirect input, vertex/fragment shader reads and transfers.
- Descriptor binding is tracked per bind point (graphics / compute), so sets survive switching between draws and dispatches. Push constants are shared.
- Sampled-read barrier scope now includes the compute stage.
- `tests/SpirvReflectTests.cpp` (dependency-free, both CMake test blocks).

## Step 5 — Vulkan: storage-buffer hazard tracking (VulkanRHI.cpp, SpirvReflect.h)
- The unconditional full memory barrier after every dispatch with storage buffers is gone. Each command list now keeps the GPU buffer accesses
  since the last buffer barrier (buffer + byte range + read/write). A barrier is emitted only before a dispatch that overlaps an earlier access
  with at least one writer (RAW / WAR / WAW). Read-only sharing, disjoint ranges and different buffers overlap freely.
- Read vs write comes from the SPIR-V: `SpirvComputeInfo::storageBufferUsedMask / storageBufferWriteMask` (set 3; `readonly` = NonWritable on the
  variable or on every member of the block; SSBO as StorageBuffer or as Uniform + BufferBlock; descriptor arrays). Anything the scanner is unsure
  about (corrupt module, OpGroupDecorate / OpDecorateId) falls back to "every binding used and written" = more barriers, never fewer.
- `dispatchIndirect` counts as a 12-byte read of its argument buffer (DRAW_INDIRECT stage is part of the compute-to-compute barrier).
- Compute writes are made visible to graphics (vertex / index / indirect / shader reads, transfers) by one barrier at `beginRenderPass`,
  and only when something was written. `end()` always closes the list with a COMPUTE -> ALL_COMMANDS barrier when accesses are open, so
  later command lists and submissions are ordered against this list's buffer accesses.
- `dynamic` (CPU-updated ring) buffers are never tracked. At most 256 open accesses are kept; past that the next dispatch synchronises.
- Tests: `SpirvReflectTests` covers used/write masks (readonly block, readonly variable, descriptor array, BufferBlock, UBO ignored, conservative fallbacks).
- Verification: `VulkanRHI.cpp` `-fsyntax-only -Wall` against system Vulkan headers + VMA 3.1.0 + GLFW 3.3.10: no errors. Not run on a GPU.

## Step 6 — Vulkan: storage-image hazard tracking (VulkanRHI.cpp, SpirvReflect.h)
- The GENERAL -> GENERAL barrier (ALL_COMMANDS both sides) that every dispatch with storage images emitted is gone. `Texture::collect` now
  only emits real layout transitions into GENERAL; ordering between dispatches comes from the same hazard tracker as buffers, per subresource
  (image + mip range + layer range, RAW / WAR / WAW), so dispatches on different images, disjoint mips/layers or read-only (`readonly`) images overlap.
- A conflict emits one COMPUTE -> COMPUTE memory barrier (buffers and images share it); `beginRenderPass` / `end()` flush as in step 5.
- `SpirvComputeInfo::storageImageUsedMask / storageImageWriteMask` (set 2, NonWritable on the variable = read-only), same conservative fallback.
- Tests: reflection of image masks added to `SpirvReflectTests`. Verification: `-fsyntax-only -Wall` clean, not run on a GPU.

## Step 7 — OpenGL compute (OpenGLRHI.h / OpenGLRHI.cpp)
- OpenGL 4.3+ (the GL backend is 4.5): `ShaderDesc::computeSource` (GLSL) -> `Shader` (compute program), `createComputePipeline`, `setComputePipeline`,
  `dispatch` (counts checked against GL_MAX_COMPUTE_WORK_GROUP_COUNT, zero = no-op), `dispatchIndirect` (`BufferUsage::Indirect`),
  `bindStorageTexture` (needs `TextureUsage::Storage`, plain 2D, one mip; moves nothing - GL has no layouts) and `bindStorageBuffer`
  (`Storage` / `Indirect`, offset alignment and range limits checked; nullptr unbinds). `caps.compute`, `caps.storageImages` and the compute limits are filled.
- GL shader flavour: `#version 430 core`, flat binding points - `layout(std140, binding = N) uniform`, `layout(binding = N) uniform sampler2D`,
  `layout(binding = N, rgba16f) uniform image2D`, `layout(std430, binding = N) buffer`. Same N ranges as RHI.h. Local size is read back from the program and validated.
- Barriers: same hazard model as Vulkan (accesses since the last barrier, barrier only before an overlapping access with a writer). GLSL has no
  reflection here, so every bound storage buffer / image counts as written; sampled textures and the indirect argument buffer count as reads.
  Conflict -> `glMemoryBarrier(SHADER_STORAGE | SHADER_IMAGE_ACCESS | TEXTURE_FETCH | COMMAND)`. Before the next draw and at `end()`, pending writes get
  `GL_ALL_BARRIER_BITS` (once per compute -> graphics transition).
- A texture sampled and bound as storage image at the same time throws `std::logic_error` (as on Vulkan). `begin()` resets storage / sampled bindings.
- Textures: new formats RG32F, RGBA32F, R16, RG16, RGBA16, R32UI, RGBA8Snorm in the GL mapping (internal / external format and data type);
  `Texture::format()` / `levels()` now report the real values. Storage usage rejects formats that are not valid GL image formats.
- Limits (GL): no compute push constants (`ComputePipelineDesc::pushConstantBytes > 0` throws `UnsupportedByBackend`; pass data in a UBO),
  no array / cube / 3D storage images (`createTexture`, `bindTextureView`, `copyTexture` are still unsupported on GL), `dynamic` buffers are not rings on GL.
- Verification: `OpenGLRHI.cpp` `-fsyntax-only -Wall -Wextra` against glad2 (GL 4.5 core): no errors or warnings. Not linked, not run.

## Limits (step 4, updated in steps 5-7)
- Compute: Vulkan and OpenGL (step 7); D3D12 still throws `UnsupportedByBackend`.
- `dynamic` buffers bound as storage are CPU-updated per-frame rings: read-only for shaders.
- Dispatch is not allowed inside a render pass.

## Verification (step 4)
- `VulkanRHI.cpp`: `-fsyntax-only -Wall -Wextra` against Vulkan-Headers 1.4.309 + VMA 3.1.0 + GLFW 3.4: no errors. RHI.h `-Werror` clean. `SpirvReflectTests` pass.
- Not linked, not run on a GPU. Build with `-DANVIL_WITH_VULKAN=ON`, run with `DeviceCreateInfo::validation = true`.

## Limits (steps 1-3)
- 3D textures cannot be render targets; compressed formats cannot be render targets or mip-generated.
- Multisampling needs a plain 2D texture.

## Verification
- RHI.h: `g++ -std=c++20 -fsyntax-only -Wall -Wextra -Werror`.
- VulkanRHI.cpp: `-fsyntax-only -Wall` against Vulkan headers + VMA 3.1.0: no errors (only `-Wextra` missing-initializer noise that already existed).
- Not linked, not run on a GPU. Build with `-DANVIL_WITH_VULKAN=ON` and run with `DeviceCreateInfo::validation = true`.
