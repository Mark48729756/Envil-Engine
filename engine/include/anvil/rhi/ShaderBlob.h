#pragma once
#include "anvil/rhi/RHI.h"
#include <string>

namespace anvil::rhi {
// Loads a vertex+fragment pair compiled by cmake/AnvilShaders.cmake from `dir` and fills the blob fields the backend needs:
//   Vulkan   -> <name>.vert.spv / <name>.frag.spv       (ShaderDesc::vertexSpirv / fragmentSpirv)
//   Direct3D12 -> <name>.vert.dxil / <name>.frag.dxil   (ShaderDesc::vertexDxil / fragmentDxil)
// `vertName` / `fragName` are the source file names, e.g. "gbuffer.vert", "gbuffer.frag". Throws std::runtime_error if a blob is missing.
ShaderDesc loadShaderDesc(Backend backend, const std::string& dir, const std::string& vertName, const std::string& fragName);
// Compute shader (v3): `compName` is e.g. "downsample.comp"; fills ShaderDesc::computeSpirv (Vulkan) / computeDxil (Direct3D12).
ShaderDesc loadComputeShaderDesc(Backend backend, const std::string& dir, const std::string& compName);
// Directory with the compiled blobs (ANVIL_COMPILED_SHADER_DIR if defined, otherwise "<ANVIL_SHADER_DIR>/compiled").
std::string compiledShaderDir();
}
