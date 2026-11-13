#include "rhi/SpirvReflect.h"
#include <cstdio>
#include <vector>
using namespace anvil::rhi;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
static std::uint32_t op(std::uint32_t code, std::uint32_t len) { return (len << 16) | code; }
static std::vector<std::uint32_t> module(std::uint32_t model, std::uint32_t mode, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::vector<std::uint32_t> m = {0x07230203u, 0x00010500u, 0, 10, 0};
    m.insert(m.end(), {op(15, 5), model, 4, 0x6E69616Du, 0});          // OpEntryPoint <model> %4 "main"
    m.insert(m.end(), {op(16, 6), 4, mode, x, y, z});                  // OpExecutionMode %4 <mode> x y z
    return m;
}
// Compute module with storage buffers in set 3 (plus a UBO in set 0 that must be ignored).
//   binding 1: SSBO, writable                         binding 2: SSBO, every member NonWritable (readonly block)
//   binding 3: array of SSBO, NonWritable on the var   binding 4: Uniform + BufferBlock (GLSL < 1.3 style), writable
static std::vector<std::uint32_t> bufferModule(bool groupDecorate) {
    auto m = module(5, 17, 1, 1, 1);
    auto var = [&](std::uint32_t type, std::uint32_t id, std::uint32_t sc, std::uint32_t set, std::uint32_t binding) {
        m.insert(m.end(), {op(71, 4), id, 34, set}); m.insert(m.end(), {op(71, 4), id, 33, binding});
        m.insert(m.end(), {op(59, 4), type, id, sc});
    };
    m.insert(m.end(), {op(72, 4), 11, 0, 24});                          // OpMemberDecorate %11 0 NonWritable
    m.insert(m.end(), {op(71, 3), 14, 3});                              // OpDecorate %14 BufferBlock (struct %14)
    m.insert(m.end(), {op(30, 3), 10, 5, op(30, 3), 11, 5, op(30, 3), 14, 5, op(30, 3), 16, 5});   // OpTypeStruct %10 %11 %14 %16 {float}
    m.insert(m.end(), {op(29, 3), 15, 10});                              // OpTypeRuntimeArray %15 %10
    m.insert(m.end(), {op(32, 4), 20, 12, 10, op(32, 4), 21, 12, 11, op(32, 4), 22, 12, 15, op(32, 4), 23, 2, 14, op(32, 4), 24, 2, 16});
    m.insert(m.end(), {op(71, 3), 32, 24});                              // OpDecorate %32 NonWritable
    var(20, 30, 12, 3, 1); var(21, 31, 12, 3, 2); var(22, 32, 12, 3, 3); var(23, 33, 2, 3, 4);
    var(24, 34, 2, 0, 0);                                                 // UBO, set 0: ignored
    m.insert(m.end(), {op(32, 4), 25, 0, 10});                           // UniformConstant pointer (stands in for a storage image type)
    var(25, 35, 0, 2, 0); var(25, 36, 0, 2, 5);                           // storage images, set 2: binding 0 writable, binding 5 readonly
    m.insert(m.end(), {op(71, 3), 36, 24});                              // OpDecorate %36 NonWritable
    var(25, 37, 0, 1, 0);                                                 // sampled texture, set 1: ignored
    if (groupDecorate) m.insert(m.end(), {op(74, 2), 99});
    return m;
}
int main() {
    auto bm = reflectSpirvCompute(bufferModule(false));
    CHECK(bm.valid && bm.storageBufferUsedMask == 0x1Eu);                 // bindings 1..4
    CHECK(bm.storageBufferWriteMask == ((1u << 1) | (1u << 4)));          // 2 and 3 are read-only
    CHECK(bm.storageImageUsedMask == ((1u << 0) | (1u << 5)) && bm.storageImageWriteMask == 1u);
    auto gm = reflectSpirvCompute(bufferModule(true));                    // unknown decoration scheme: stay conservative
    CHECK(gm.valid && gm.storageBufferUsedMask == 0xFFFFFFFFu && gm.storageBufferWriteMask == 0xFFFFFFFFu && gm.storageImageWriteMask == 0xFFFFFFFFu);
    auto nb = reflectSpirvCompute(module(5, 17, 8, 1, 1));                // no buffers at all
    CHECK(nb.valid && nb.storageBufferUsedMask == 0 && nb.storageBufferWriteMask == 0 && nb.storageImageUsedMask == 0);
    CHECK(reflectSpirvCompute(nullptr, 0).storageBufferWriteMask == 0xFFFFFFFFu);   // invalid: conservative
    auto a = reflectSpirvCompute(module(5, 17, 8, 4, 2));
    CHECK(a.valid && a.hasComputeEntry && a.localSizeKnown && a.localSize[0] == 8 && a.localSize[1] == 4 && a.localSize[2] == 2);
    auto b = reflectSpirvCompute(module(0, 17, 8, 8, 1));            // vertex entry point
    CHECK(!b.valid && !b.hasComputeEntry);
    auto c = reflectSpirvCompute(module(5, 38, 1, 2, 3));            // LocalSizeId: not a literal size
    CHECK(c.valid && !c.localSizeKnown && c.localSize[0] == 1);
    auto d = module(5, 17, 8, 8, 1); d.pop_back();                   // truncated
    CHECK(!reflectSpirvCompute(d).valid);
    CHECK(!reflectSpirvCompute(nullptr, 0).valid);
    std::vector<std::uint32_t> bad = {1, 2, 3, 4, 5};
    CHECK(!reflectSpirvCompute(bad).valid);
    if (!fails) std::printf("SpirvReflectTests passed\n");
    return fails;
}
