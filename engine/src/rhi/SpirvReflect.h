#pragma once
// Minimal SPIR-V scanner used by the Vulkan backend to learn the compute entry point and its local workgroup size
// (OpExecutionMode LocalSize). Header-only, no Vulkan dependency, so it is unit-tested on its own (tests/SpirvReflectTests.cpp).
//
// Supported: LocalSize (literal) and, for descriptor set 3, which storage buffers the module declares and which of them it may write
// (NonWritable on the variable, or on every member of its block, means read-only). Not supported: LocalSizeId / SpecId-driven sizes
// (reported as unknown, the caller falls back to 1x1x1 + a warning). Everything else in the module is skipped.
// The buffer masks are conservative: whenever the scanner cannot be sure (corrupt module, OpGroupDecorate / OpDecorateId) every
// binding counts as used and writable, so a caller that skips barriers based on them can only err towards too many barriers.
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace anvil::rhi {

struct SpirvComputeInfo {
    bool valid = false;               // well-formed module with a GLCompute entry point
    bool hasComputeEntry = false;
    bool localSizeKnown = false;      // LocalSize literal found (false for LocalSizeId / spec constants)
    std::uint32_t localSize[3] = {1, 1, 1};
    // Descriptor set 3 (storage buffers), bit n = binding n (bindings >= 32 are ignored; the backend only has 8).
    std::uint32_t storageBufferUsedMask = 0xFFFFFFFFu;    // bindings the module declares
    std::uint32_t storageBufferWriteMask = 0xFFFFFFFFu;   // subset it may write (not read-only)
    // Descriptor set 2 (storage images), same encoding.
    std::uint32_t storageImageUsedMask = 0xFFFFFFFFu;
    std::uint32_t storageImageWriteMask = 0xFFFFFFFFu;
};

inline SpirvComputeInfo reflectSpirvCompute(const std::uint32_t* words, std::size_t count) {
    constexpr std::uint32_t kMagic = 0x07230203u;
    constexpr std::uint32_t opEntryPoint = 15, opExecutionMode = 16;
    constexpr std::uint32_t execGLCompute = 5;
    constexpr std::uint32_t modeLocalSize = 17;
    constexpr std::uint32_t opTypeStruct = 30, opTypeArray = 28, opTypeRuntimeArray = 29, opTypePointer = 32, opVariable = 59;
    constexpr std::uint32_t opDecorate = 71, opMemberDecorate = 72, opGroupDecorate = 74, opGroupMemberDecorate = 75, opDecorateId = 332;
    constexpr std::uint32_t decBufferBlock = 3, decNonWritable = 24, decBinding = 33, decDescriptorSet = 34;
    constexpr std::uint32_t scUniform = 2, scStorageBuffer = 12;
    constexpr std::uint32_t kStorageBufferSet = 3, kStorageImageSet = 2, scUniformConstant = 0;
    SpirvComputeInfo r;
    if (!words || count < 5 || words[0] != kMagic) return r;
    std::uint32_t entryId = 0;
    bool haveEntry = false;
    bool unsure = false;   // constructs the buffer scan does not understand
    struct Var { std::uint32_t type, id, storageClass; };
    std::vector<Var> vars;
    std::unordered_map<std::uint32_t, std::uint32_t> setOf, bindingOf, structMembers, nonWritableMembers, pointee, element;
    std::unordered_map<std::uint32_t, bool> nonWritableVar, bufferBlock;
    std::size_t i = 5;   // header: magic, version, generator, bound, schema
    while (i < count) {
        const std::uint32_t head = words[i];
        const std::uint32_t op = head & 0xFFFFu, len = head >> 16;
        if (len == 0 || i + len > count) return r;   // truncated / corrupt
        if (op == opEntryPoint && len >= 4 && words[i + 1] == execGLCompute && !haveEntry) {
            haveEntry = true; entryId = words[i + 2];   // first compute entry point (the backend always uses "main")
        } else if (op == opExecutionMode && len >= 6 && haveEntry && words[i + 1] == entryId && words[i + 2] == modeLocalSize) {
            for (int k = 0; k < 3; ++k) r.localSize[k] = words[i + 3 + k];
            r.localSizeKnown = r.localSize[0] > 0 && r.localSize[1] > 0 && r.localSize[2] > 0;
        }
        switch (op) {
            case opDecorate:
                if (len >= 3) {
                    const std::uint32_t t = words[i + 1], dec = words[i + 2];
                    if (dec == decDescriptorSet && len >= 4) setOf[t] = words[i + 3];
                    else if (dec == decBinding && len >= 4) bindingOf[t] = words[i + 3];
                    else if (dec == decNonWritable) nonWritableVar[t] = true;
                    else if (dec == decBufferBlock) bufferBlock[t] = true;
                }
                break;
            case opMemberDecorate:
                if (len >= 4 && words[i + 3] == decNonWritable) ++nonWritableMembers[words[i + 1]];
                break;
            case opGroupDecorate: case opGroupMemberDecorate: case opDecorateId: unsure = true; break;
            case opTypeStruct: if (len >= 2) structMembers[words[i + 1]] = len - 2; break;
            case opTypeArray: case opTypeRuntimeArray: if (len >= 3) element[words[i + 1]] = words[i + 2]; break;
            case opTypePointer: if (len >= 4) pointee[words[i + 1]] = words[i + 3]; break;
            case opVariable: if (len >= 4) vars.push_back({words[i + 1], words[i + 2], words[i + 3]}); break;
            default: break;
        }
        i += len;
    }
    r.hasComputeEntry = haveEntry;
    r.valid = haveEntry;
    if (haveEntry && !unsure) {
        std::uint32_t used = 0, write = 0;
        for (const Var& v : vars) {
            auto s = setOf.find(v.id); auto b = bindingOf.find(v.id);
            if (s == setOf.end() || b == bindingOf.end() || s->second != kStorageBufferSet || b->second >= 32) continue;
            auto p = pointee.find(v.type);
            if (p == pointee.end()) continue;
            std::uint32_t t = p->second;                                   // peel descriptor arrays
            for (auto e = element.find(t); e != element.end(); e = element.find(t)) t = e->second;
            const bool ssbo = v.storageClass == scStorageBuffer || (v.storageClass == scUniform && bufferBlock.count(t));
            if (!ssbo) continue;
            const std::uint32_t bit = 1u << b->second;
            used |= bit;
            auto sm = structMembers.find(t); auto nw = nonWritableMembers.find(t);
            const bool readOnly = nonWritableVar.count(v.id) || (sm != structMembers.end() && sm->second > 0 && nw != nonWritableMembers.end() && nw->second == sm->second);
            if (!readOnly) write |= bit;
        }
        r.storageBufferUsedMask = used; r.storageBufferWriteMask = write;
        // Set 2: every UniformConstant variable is a storage image; `readonly` puts NonWritable on the variable.
        std::uint32_t iused = 0, iwrite = 0;
        for (const Var& v : vars) {
            auto s = setOf.find(v.id); auto b = bindingOf.find(v.id);
            if (v.storageClass != scUniformConstant || s == setOf.end() || b == bindingOf.end() || s->second != kStorageImageSet || b->second >= 32) continue;
            const std::uint32_t bit = 1u << b->second;
            iused |= bit;
            if (!nonWritableVar.count(v.id)) iwrite |= bit;
        }
        r.storageImageUsedMask = iused; r.storageImageWriteMask = iwrite;
    }
    return r;
}

inline SpirvComputeInfo reflectSpirvCompute(const std::vector<std::uint32_t>& words) { return reflectSpirvCompute(words.data(), words.size()); }

} // namespace anvil::rhi
