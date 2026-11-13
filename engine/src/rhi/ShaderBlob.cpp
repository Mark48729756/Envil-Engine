#include "anvil/rhi/ShaderBlob.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace anvil::rhi {
namespace {
std::vector<std::uint8_t> readBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open compiled shader: " + path + " (build the 'anvil_shaders' target first)");
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
std::vector<std::uint32_t> readSpirv(const std::string& path) {
    const auto bytes = readBytes(path);
    if (bytes.empty() || bytes.size() % 4 != 0) throw std::runtime_error("Invalid SPIR-V blob: " + path);
    std::vector<std::uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    if (words[0] != 0x07230203u) throw std::runtime_error("Not a SPIR-V module (bad magic): " + path);
    return words;
}
}
std::string compiledShaderDir() {
#if defined(ANVIL_COMPILED_SHADER_DIR)
    return ANVIL_COMPILED_SHADER_DIR;
#elif defined(ANVIL_SHADER_DIR)
    return std::string(ANVIL_SHADER_DIR) + "/compiled";
#else
    return "shaders/compiled";
#endif
}
ShaderDesc loadShaderDesc(Backend backend, const std::string& dir, const std::string& vertName, const std::string& fragName) {
    ShaderDesc d; d.label = vertName + "+" + fragName;
    const std::string base = dir + "/";
    switch (backend) {
        case Backend::Vulkan12:
        case Backend::Vulkan14:
            d.vertexSpirv = readSpirv(base + vertName + ".spv");
            d.fragmentSpirv = readSpirv(base + fragName + ".spv");
            break;
        case Backend::Direct3D12:
            d.vertexDxil = readBytes(base + vertName + ".dxil");
            d.fragmentDxil = readBytes(base + fragName + ".dxil");
            break;
        case Backend::OpenGL45:
            throw std::invalid_argument("OpenGL uses GLSL source (ShaderDesc::vertexSource/fragmentSource), not compiled blobs");
    }
    return d;
}
ShaderDesc loadComputeShaderDesc(Backend backend, const std::string& dir, const std::string& compName) {
    ShaderDesc d; d.label = compName;
    const std::string base = dir + "/";
    switch (backend) {
        case Backend::Vulkan12:
        case Backend::Vulkan14: d.computeSpirv = readSpirv(base + compName + ".spv"); break;
        case Backend::Direct3D12: d.computeDxil = readBytes(base + compName + ".dxil"); break;
        case Backend::OpenGL45: throw std::invalid_argument("OpenGL uses GLSL source (ShaderDesc::computeSource), not compiled blobs");
    }
    return d;
}
}
