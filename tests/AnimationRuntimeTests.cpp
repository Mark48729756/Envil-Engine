#include "anvil/AnimationRuntime.h"
#include <cassert>
#include <cmath>
#include <vector>
using namespace anvil;
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool nearV(Vec3 a, Vec3 b, float e = 1e-4f) { return near(a.x,b.x,e) && near(a.y,b.y,e) && near(a.z,b.z,e); }
static Skeleton makeChain() {
    Bone root; root.name = "root";
    Bone upper; upper.name = "upper"; upper.parent = 0; upper.bindPose.translation = {0,1,0};
    Bone lower; lower.name = "lower"; lower.parent = 1; lower.bindPose.translation = {0,1,0};
    return Skeleton({root, upper, lower});
}
int main() {
    // quaternion helpers: +90 deg about Y maps +X to -Z
    assert(nearV(quatRotate(quatFromAxisAngle({0,1,0}, 1.5707963f), {1,0,0}), {0,0,-1}));

    // matrix inverse round-trip with rotation + non-uniform scale + translation
    BoneTransform t; t.translation = {1,-2,3}; t.rotation = quatFromAxisAngle({1,1,0}, 0.7f); t.scale = {2,0.5f,3};
    AnimMat4 prod = matFromTransform(t) * inverseAffine(matFromTransform(t));
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) assert(near(prod.at(r,c), r == c ? 1.0f : 0.0f));

    // bind pose -> identity palette
    Skeleton sk = makeChain();
    assert(findBoneIndex(sk, "lower") == 2 && findBoneIndex(sk, "nope") == -1);
    auto invBind = computeInverseBindPose(sk);
    std::vector<BoneTransform> local(3);
    for (std::size_t i = 0; i < 3; ++i) local[i] = sk.bones()[i].bindPose;
    std::vector<AnimMat4> palette;
    buildSkinningPalette(sk, local, invBind, palette);
    for (const auto& m : palette) for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) assert(near(m.at(r,c), r == c ? 1.0f : 0.0f));

    // rotate "upper" +90 deg about Z: lower joint (0,2,0) -> (-1,1,0); point (0,1.5,0) on upper -> (-0.5,1,0)
    local[1].rotation = quatFromAxisAngle({0,0,1}, 1.5707963f);
    buildSkinningPalette(sk, local, invBind, palette);
    assert(nearV(transformPoint(palette[2], {0,2,0}), {-1,1,0}));
    assert(nearV(transformPoint(palette[1], {0,1.5f,0}), {-0.5f,1,0}));
    assert(nearV(transformPoint(palette[0], {0,0.3f,0}), {0,0.3f,0}));
    // column-major layout contract for GLSL: translation lives in m[12..14]
    assert(near(palette[2].m[12], 0.0f + palette[2].at(0,3)) && near(palette[2].m[13], palette[2].at(1,3)));

    // events
    AnimationClip ev; ev.duration = 1; ev.events = {{0.25f,"a"},{0.75f,"b"},{0.0f,"start"}};
    std::vector<const AnimationEvent*> out;
    collectEvents(ev, 0.9f, 1.3f, true, out); // wrap: "start" at t=1.0, then "a" at t=1.25
    assert(out.size() == 2 && out[0]->name == "start" && out[1]->name == "a");
    out.clear(); collectEvents(ev, 0.0f, 2.5f, true, out); assert(out.size() == 7);   // start@1,2  a@0.25,1.25,2.25  b@0.75,1.75
    out.clear(); collectEvents(ev, 0.0f, 2.5f, true, out, true);
    assert(out.size() == 8 && out.front()->name == "start" && out.back()->name == "a");
    out.clear(); collectEvents(ev, 0.5f, 5.0f, false, out); assert(out.size() == 1 && out[0]->name == "b");
    out.clear(); collectEvents(ev, 1.0f, 3.0f, false, out); assert(out.empty());

    // root motion: x goes 0 -> 4 over 2 s (2 m/s)
    AnimationClip walk; walk.duration = 2; walk.tracks.resize(1);
    BoneTransform p0, p1; p1.translation.x = 4;
    walk.tracks[0].keys = {{0,p0},{2,p1}};
    assert(near(extractRootMotion(walk, 0, 0.5f, 1.0f, true).translation.x, 1.0f));
    assert(near(extractRootMotion(walk, 0, 1.5f, 2.5f, true).translation.x, 2.0f));   // across a wrap
    assert(near(extractRootMotion(walk, 0, 0.0f, 5.0f, true).translation.x, 10.0f));  // several cycles
    assert(near(extractRootMotion(walk, 0, 1.0f, 9.0f, false).translation.x, 2.0f));  // non-loop clamps at the end
    assert(extractRootMotion(walk, 5, 0, 1, true).translation.x == 0.0f);             // bad bone index
    // translation is reported in the root's local frame at interval start (root yawed +90 deg about Y)
    AnimationClip turned; turned.duration = 1; turned.tracks.resize(1);
    BoneTransform a, b; a.rotation = b.rotation = quatFromAxisAngle({0,1,0}, 1.5707963f); b.translation.x = 1;
    turned.tracks[0].keys = {{0,a},{1,b}};
    assert(nearV(extractRootMotion(turned, 0, 0, 1, false).translation, {0,0,1}));
    // rotation delta: yaw 0 -> 180 over 1 s
    AnimationClip spin; spin.duration = 1; spin.tracks.resize(1);
    BoneTransform s0, s1; s1.rotation = quatFromAxisAngle({0,1,0}, 3.1415926f);
    spin.tracks[0].keys = {{0,s0},{1,s1}};
    Quaternion d = extractRootMotion(spin, 0, 0, 1, false).rotation;
    assert(near(std::fabs(d.y), 1.0f, 1e-3f) && near(d.w, 0.0f, 1e-3f));
}
