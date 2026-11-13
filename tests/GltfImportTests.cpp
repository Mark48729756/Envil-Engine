#include "anvil/GltfImport.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
using namespace anvil;
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool nearV(Vec3 a, Vec3 b, float e = 1e-4f) { return near(a.x,b.x,e) && near(a.y,b.y,e) && near(a.z,b.z,e); }
// Test asset (generated): 3 joints listed in scrambled order [lower, root, upper], 1 skinned triangle, 3 clips.
static const unsigned char kBin[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,0,1,0,0,0,2,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,2,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,192,0,0,0,0,0,0,128,63,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,128,191,0,0,0,0,0,0,128,63,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,0,0,0,0,0,0,243,4,53,63,243,4,53,63,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,63,0,0,128,63,0,0,128,63,0,0,0,64,0,0,0,64,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,64,0,0,64,64,0,0,64,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,64,0,0,0,0,0,0,0,0};
static const char kGltf[] = R"JSON({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,1]}],"nodes":[{"name":"mesh","mesh":0,"skin":0},{"name":"root","children":[2]},{"name":"upper","translation":[0,1,0],"children":[3]},{"name":"lower","translation":[0,1,0]}],"skins":[{"joints":[3,1,2],"inverseBindMatrices":4}],"meshes":[{"name":"arm","primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],"animations":[{"name":"bend","samplers":[{"input":5,"output":6,"interpolation":"LINEAR"},{"input":7,"output":8}],"channels":[{"sampler":0,"target":{"node":2,"path":"rotation"}},{"sampler":1,"target":{"node":1,"path":"translation"}}]},{"name":"steps","samplers":[{"input":5,"output":10,"interpolation":"STEP"}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},{"name":"grow","samplers":[{"input":5,"output":9,"interpolation":"CUBICSPLINE"}],"channels":[{"sampler":0,"target":{"node":3,"path":"scale"}}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":4,"componentType":5126,"count":3,"type":"MAT4"},{"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":6,"componentType":5126,"count":2,"type":"VEC4"},{"bufferView":7,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":8,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":9,"componentType":5126,"count":6,"type":"VEC3"},{"bufferView":10,"componentType":5126,"count":2,"type":"VEC3"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":12},{"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":6},{"buffer":0,"byteOffset":104,"byteLength":192},{"buffer":0,"byteOffset":296,"byteLength":8},{"buffer":0,"byteOffset":304,"byteLength":32},{"buffer":0,"byteOffset":336,"byteLength":8},{"buffer":0,"byteOffset":344,"byteLength":24},{"buffer":0,"byteOffset":368,"byteLength":72},{"buffer":0,"byteOffset":440,"byteLength":24}],"buffers":[{"uri":"arm.bin","byteLength":464}]})JSON";
static const char kGlbJson[] = R"JSON({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,1]}],"nodes":[{"name":"mesh","mesh":0,"skin":0},{"name":"root","children":[2]},{"name":"upper","translation":[0,1,0],"children":[3]},{"name":"lower","translation":[0,1,0]}],"skins":[{"joints":[3,1,2],"inverseBindMatrices":4}],"meshes":[{"name":"arm","primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],"animations":[{"name":"bend","samplers":[{"input":5,"output":6,"interpolation":"LINEAR"},{"input":7,"output":8}],"channels":[{"sampler":0,"target":{"node":2,"path":"rotation"}},{"sampler":1,"target":{"node":1,"path":"translation"}}]},{"name":"steps","samplers":[{"input":5,"output":10,"interpolation":"STEP"}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},{"name":"grow","samplers":[{"input":5,"output":9,"interpolation":"CUBICSPLINE"}],"channels":[{"sampler":0,"target":{"node":3,"path":"scale"}}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":4,"componentType":5126,"count":3,"type":"MAT4"},{"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":6,"componentType":5126,"count":2,"type":"VEC4"},{"bufferView":7,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":8,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":9,"componentType":5126,"count":6,"type":"VEC3"},{"bufferView":10,"componentType":5126,"count":2,"type":"VEC3"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":12},{"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":6},{"buffer":0,"byteOffset":104,"byteLength":192},{"buffer":0,"byteOffset":296,"byteLength":8},{"buffer":0,"byteOffset":304,"byteLength":32},{"buffer":0,"byteOffset":336,"byteLength":8},{"buffer":0,"byteOffset":344,"byteLength":24},{"buffer":0,"byteOffset":368,"byteLength":72},{"buffer":0,"byteOffset":440,"byteLength":24}],"buffers":[{"byteLength":464}]})JSON";

static std::vector<std::uint8_t> makeGlb() {
    std::string json = kGlbJson; while (json.size() % 4) json += ' ';
    std::vector<std::uint8_t> bin(kBin, kBin + sizeof(kBin)); while (bin.size() % 4) bin.push_back(0);
    std::vector<std::uint8_t> out; auto w32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8*i))); };
    w32(0x46546C67u); w32(2); w32(static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
    w32(static_cast<std::uint32_t>(json.size())); w32(0x4E4F534Au); out.insert(out.end(), json.begin(), json.end());
    w32(static_cast<std::uint32_t>(bin.size())); w32(0x004E4942u); out.insert(out.end(), bin.begin(), bin.end());
    return out;
}
static void checkResult(const GltfImportResult& r) {
    assert(r.ok);
    assert(r.bones.size() == 3);
    assert(r.bones[0].name == "root" && r.bones[1].name == "upper" && r.bones[2].name == "lower");   // reordered parent-first
    assert(r.bones[0].parent == -1 && r.bones[1].parent == 0 && r.bones[2].parent == 1);
    assert(nearV(r.bones[1].bindPose.translation, {0,1,0}));
    Skeleton sk(r.bones);                                   // must not throw
    assert(r.inverseBind.size() == 3);
    assert(nearV(transformPoint(r.inverseBind[2], {0,2,0}), {0,0,0}));   // lower joint IBM was stored in slot 0
    assert(nearV(transformPoint(r.inverseBind[1], {0,1,0}), {0,0,0}));
    // mesh + joint remap (file slots 1,2,0 -> bones 0,1,2)
    assert(r.meshes.size() == 1 && r.meshes[0].vertices.size() == 3 && r.meshes[0].indices.size() == 3);
    assert(r.meshes[0].vertices[0].joints[0] == 0 && r.meshes[0].vertices[1].joints[0] == 1 && r.meshes[0].vertices[2].joints[0] == 2);
    assert(near(r.meshes[0].vertices[2].weights[0], 1.0f) && nearV(r.meshes[0].vertices[2].position, {0,2,0}));
    // clips
    assert(r.clips.size() == 3 && r.clips[0].name == "bend" && r.clips[0].tracks.size() == 3);
    const AnimationClip& bend = r.clips[0];
    assert(near(bend.duration, 2.0f));
    assert(nearV(sampleTrack(bend.tracks[0], 1.0f).translation, {1,0,0}));
    const BoneTransform up = sampleTrack(bend.tracks[1], 1.0f);
    assert(near(up.rotation.z, std::sin(3.1415926f/4), 1e-3f) && nearV(up.translation, {0,1,0}));      // keeps bind translation
    assert(bend.tracks[2].keys.empty());
    // STEP stays a step
    const AnimationClip& steps = r.clips[1];
    assert(near(sampleTrack(steps.tracks[0], 0.5f).translation.x, 0.0f, 1e-3f) && near(sampleTrack(steps.tracks[0], 1.0f).translation.x, 5.0f));
    // CUBICSPLINE baked: hermite(1,3, m0=2, m1=0) at s=0.5 -> 2.25 (a plain lerp would give 2.0)
    assert(near(sampleTrack(r.clips[2].tracks[2], 0.5f).scale.x, 2.25f, 1e-3f));
    // end-to-end: sample "bend" at t=1, build palette; lower joint (0,2,0): root +1 in x, upper rotated 90 deg about Z
    std::vector<BoneTransform> local(3);
    for (std::size_t i = 0; i < 3; ++i) local[i] = sampleTrack(bend.tracks[i], 1.0f, sk.bones()[i].bindPose);
    std::vector<AnimMat4> palette; buildSkinningPalette(sk, local, r.inverseBind, palette);
    assert(nearV(transformPoint(palette[2], {0,2,0}), {0,1,0}, 1e-3f));
}
int main() {
    // GLB from memory
    const auto glb = makeGlb();
    checkResult(importGltfFromMemory(glb.data(), glb.size()));
    // .gltf + external .bin on disk
    const auto dir = std::filesystem::temp_directory_path() / "anvil_gltf_test";
    std::filesystem::create_directories(dir);
    { std::ofstream(dir / "arm.bin", std::ios::binary).write(reinterpret_cast<const char*>(kBin), sizeof(kBin)); }
    { std::ofstream(dir / "arm.gltf") << kGltf; }
    checkResult(importGltfFile(dir / "arm.gltf"));
    // path traversal in buffer uri must be rejected
    std::string evil = kGltf; evil.replace(evil.find("arm.bin"), 7, "../arm.bin");
    auto bad = importGltfFromMemory(reinterpret_cast<const std::uint8_t*>(evil.data()), evil.size(), dir);
    assert(!bad.ok && bad.error.find("unsafe") != std::string::npos);
    // garbage / truncated input fails cleanly
    const char junk[] = "{\"nodes\": [";
    assert(!importGltfFromMemory(reinterpret_cast<const std::uint8_t*>(junk), sizeof(junk) - 1).ok);
    auto cut = glb; cut.resize(40);
    assert(!importGltfFromMemory(cut.data(), cut.size()).ok);
    // base64 + data URI
    std::vector<std::uint8_t> dec; assert(gltf_detail::base64Decode("TWFu", dec) && dec.size() == 3 && dec[0] == 'M' && dec[2] == 'n');
    assert(gltf_detail::base64Decode("TWE=", dec) && dec.size() == 2);
    // matrix decomposition round trip
    BoneTransform t; t.translation = {1,2,3}; t.rotation = quatFromAxisAngle({0,1,1}, 0.9f); t.scale = {2,3,0.5f};
    const BoneTransform d = transformFromMatrix(matFromTransform(t));
    assert(nearV(d.translation, t.translation) && nearV(d.scale, t.scale, 1e-3f));
    assert(near(std::fabs(d.rotation.x*t.rotation.x + d.rotation.y*t.rotation.y + d.rotation.z*t.rotation.z + d.rotation.w*t.rotation.w), 1.0f, 1e-3f));
    std::filesystem::remove_all(dir);
}
