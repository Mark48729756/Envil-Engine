#pragma once
// ANVIL M116 - Animation Core v2 (stage A1): pose -> skinning pipeline, animation events, root motion.
// Header-only, depends only on Animation.h. Matrices are column-major (m[col*4+row]), matching GLSL mat4
// so a palette can be uploaded to the std430 BonePalette buffer of shaders/rhi/skinning.vert unchanged.
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace anvil {

// ---------------------------------------------------------------- quaternion helpers
inline Quaternion quatMul(Quaternion a, Quaternion b) {
    return {a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
            a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
            a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
            a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z};
}
inline Quaternion quatConjugate(Quaternion q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Quaternion quatFromAxisAngle(Vec3 axis, float radians) {
    const float l = std::sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
    if (l < 1e-8f) return {};
    const float s = std::sin(radians * 0.5f) / l;
    return {axis.x*s, axis.y*s, axis.z*s, std::cos(radians * 0.5f)};
}
inline Vec3 quatRotate(Quaternion q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t{2*(u.y*v.z - u.z*v.y), 2*(u.z*v.x - u.x*v.z), 2*(u.x*v.y - u.y*v.x)};
    return {v.x + q.w*t.x + (u.y*t.z - u.z*t.y),
            v.y + q.w*t.y + (u.z*t.x - u.x*t.z),
            v.z + q.w*t.z + (u.x*t.y - u.y*t.x)};
}

// ---------------------------------------------------------------- 4x4 affine matrix
struct AnimMat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float& at(int row, int col) { return m[col*4 + row]; }
    float at(int row, int col) const { return m[col*4 + row]; }
};
inline AnimMat4 operator*(const AnimMat4& a, const AnimMat4& b) {
    AnimMat4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, c);
            r.at(row, c) = sum;
        }
    return r;
}
inline AnimMat4 matFromTransform(const BoneTransform& t) {
    const Quaternion q = normalize(t.rotation);
    const float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z, xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z, wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    AnimMat4 r;
    r.at(0,0) = (1 - 2*(yy+zz)) * t.scale.x; r.at(0,1) = 2*(xy - wz) * t.scale.y;       r.at(0,2) = 2*(xz + wy) * t.scale.z;       r.at(0,3) = t.translation.x;
    r.at(1,0) = 2*(xy + wz) * t.scale.x;     r.at(1,1) = (1 - 2*(xx+zz)) * t.scale.y;   r.at(1,2) = 2*(yz - wx) * t.scale.z;       r.at(1,3) = t.translation.y;
    r.at(2,0) = 2*(xz - wy) * t.scale.x;     r.at(2,1) = 2*(yz + wx) * t.scale.y;       r.at(2,2) = (1 - 2*(xx+yy)) * t.scale.z;   r.at(2,3) = t.translation.z;
    return r;
}
// Inverse of an affine matrix (bottom row 0,0,0,1). Singular matrices return identity.
inline AnimMat4 inverseAffine(const AnimMat4& a) {
    const float a00=a.at(0,0), a01=a.at(0,1), a02=a.at(0,2), a10=a.at(1,0), a11=a.at(1,1), a12=a.at(1,2), a20=a.at(2,0), a21=a.at(2,1), a22=a.at(2,2);
    const float c00 = a11*a22 - a12*a21, c01 = a12*a20 - a10*a22, c02 = a10*a21 - a11*a20;
    const float det = a00*c00 + a01*c01 + a02*c02;
    if (!std::isfinite(det) || std::fabs(det) < 1e-12f) return {};
    const float id = 1.0f / det;
    AnimMat4 r;
    r.at(0,0) = c00*id;                     r.at(0,1) = (a02*a21 - a01*a22)*id; r.at(0,2) = (a01*a12 - a02*a11)*id;
    r.at(1,0) = c01*id;                     r.at(1,1) = (a00*a22 - a02*a20)*id; r.at(1,2) = (a02*a10 - a00*a12)*id;
    r.at(2,0) = c02*id;                     r.at(2,1) = (a01*a20 - a00*a21)*id; r.at(2,2) = (a00*a11 - a01*a10)*id;
    const float tx=a.at(0,3), ty=a.at(1,3), tz=a.at(2,3);
    r.at(0,3) = -(r.at(0,0)*tx + r.at(0,1)*ty + r.at(0,2)*tz);
    r.at(1,3) = -(r.at(1,0)*tx + r.at(1,1)*ty + r.at(1,2)*tz);
    r.at(2,3) = -(r.at(2,0)*tx + r.at(2,1)*ty + r.at(2,2)*tz);
    return r;
}
inline Vec3 transformPoint(const AnimMat4& m, Vec3 v) {
    return {m.at(0,0)*v.x + m.at(0,1)*v.y + m.at(0,2)*v.z + m.at(0,3),
            m.at(1,0)*v.x + m.at(1,1)*v.y + m.at(1,2)*v.z + m.at(1,3),
            m.at(2,0)*v.x + m.at(2,1)*v.y + m.at(2,2)*v.z + m.at(2,3)};
}

// ---------------------------------------------------------------- skeleton -> skinning
inline int findBoneIndex(const Skeleton& skeleton, const std::string& name) {
    for (std::size_t i = 0; i < skeleton.size(); ++i) if (skeleton.bones()[i].name == name) return static_cast<int>(i);
    return -1;
}
// Local (parent-space) pose -> model-space matrices. Missing entries fall back to the bind pose.
// Skeleton guarantees parents precede children, so one forward pass is enough.
inline std::vector<AnimMat4> computeModelPose(const Skeleton& skeleton, const std::vector<BoneTransform>& local) {
    std::vector<AnimMat4> model(skeleton.size());
    for (std::size_t i = 0; i < skeleton.size(); ++i) {
        const Bone& b = skeleton.bones()[i];
        const AnimMat4 l = matFromTransform(i < local.size() ? local[i] : b.bindPose);
        model[i] = b.parent >= 0 ? model[static_cast<std::size_t>(b.parent)] * l : l;
    }
    return model;
}
inline std::vector<AnimMat4> computeInverseBindPose(const Skeleton& skeleton) {
    std::vector<BoneTransform> bind(skeleton.size());
    for (std::size_t i = 0; i < skeleton.size(); ++i) bind[i] = skeleton.bones()[i].bindPose;
    std::vector<AnimMat4> model = computeModelPose(skeleton, bind);
    for (auto& m : model) m = inverseAffine(m);
    return model;
}
// palette[i] = model[i] * inverseBind[i]; this is what the skinning vertex shader consumes.
inline void buildSkinningPalette(const Skeleton& skeleton, const std::vector<BoneTransform>& local,
                                 const std::vector<AnimMat4>& inverseBind, std::vector<AnimMat4>& palette) {
    const std::vector<AnimMat4> model = computeModelPose(skeleton, local);
    palette.resize(model.size());
    for (std::size_t i = 0; i < model.size(); ++i) palette[i] = i < inverseBind.size() ? model[i] * inverseBind[i] : model[i];
}

// ---------------------------------------------------------------- animation events (notifies)
// prevTime/currTime are monotonic (unwrapped) playback times. Fires events in (prevTime, currTime];
// includeStart makes the lower bound inclusive (use on the first update after play()).
// Looping clips are handled across any number of wraps; events come out in chronological order.
inline void collectEvents(const AnimationClip& clip, float prevTime, float currTime, bool loop,
                          std::vector<const AnimationEvent*>& out, bool includeStart = false) {
    if (!(currTime > prevTime) || !std::isfinite(prevTime) || !std::isfinite(currTime)) {
        if (!(includeStart && currTime == prevTime)) return;
    }
    const float dur = clip.duration;
    std::vector<std::pair<float, const AnimationEvent*>> hits;
    auto inWindow = [&](float t) { return (includeStart ? t >= prevTime : t > prevTime) && t <= currTime; };
    if (loop && dur > 1e-6f) {
        long k0 = static_cast<long>(std::floor(prevTime / dur)), k1 = static_cast<long>(std::floor(currTime / dur));
        if (k1 - k0 > 1024) k0 = k1 - 1024;
        for (long k = k0; k <= k1; ++k)
            for (const auto& e : clip.events) {
                float et = std::fmod(std::max(0.0f, e.time), dur);
                const float t = et + static_cast<float>(k) * dur;
                if (inWindow(t)) hits.emplace_back(t, &e);
            }
    } else {
        for (const auto& e : clip.events) {
            const float t = std::clamp(e.time, 0.0f, std::max(0.0f, dur));
            if (inWindow(t)) hits.emplace_back(t, &e);
        }
    }
    std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& h : hits) out.push_back(h.second);
}

// ---------------------------------------------------------------- root motion
// Delta of the root bone between two playback times. Translation is expressed in the root's local frame at
// the start of the interval, so a character controller can apply it as: pos += facing.rotate(delta.translation).
struct RootMotionDelta { Vec3 translation{}; Quaternion rotation{}; };

inline RootMotionDelta composeRootMotion(const RootMotionDelta& first, const RootMotionDelta& second) {
    const Vec3 r = quatRotate(first.rotation, second.translation);
    return {{first.translation.x + r.x, first.translation.y + r.y, first.translation.z + r.z},
            normalize(quatMul(first.rotation, second.rotation))};
}
namespace detail {
inline RootMotionDelta rootSegment(const BoneTrack& track, float a, float b, const BoneTransform& fallback) {
    const BoneTransform s0 = sampleTrack(track, a, fallback), s1 = sampleTrack(track, b, fallback);
    const Quaternion q0 = normalize(s0.rotation), inv0 = quatConjugate(q0);
    const Vec3 d{s1.translation.x - s0.translation.x, s1.translation.y - s0.translation.y, s1.translation.z - s0.translation.z};
    return {quatRotate(inv0, d), normalize(quatMul(inv0, normalize(s1.rotation)))};
}
} // namespace detail

inline RootMotionDelta extractRootMotion(const AnimationClip& clip, std::size_t rootBone, float prevTime, float currTime,
                                         bool loop, const BoneTransform& fallback = {}) {
    if (rootBone >= clip.tracks.size() || !(currTime > prevTime) || !std::isfinite(prevTime) || !std::isfinite(currTime)) return {};
    const BoneTrack& track = clip.tracks[rootBone];
    const float dur = clip.duration;
    if (!loop || dur <= 1e-6f) {
        const float hi = std::max(0.0f, dur);
        return detail::rootSegment(track, std::clamp(prevTime, 0.0f, hi), std::clamp(currTime, 0.0f, hi), fallback);
    }
    const float cp = std::floor(prevTime / dur), cc = std::floor(currTime / dur);
    const long cycles = static_cast<long>(cc - cp);
    const float a = prevTime - cp * dur, b = currTime - cc * dur;
    if (cycles == 0) return detail::rootSegment(track, a, b, fallback);
    RootMotionDelta total = detail::rootSegment(track, a, dur, fallback);
    if (cycles > 1) {
        const RootMotionDelta full = detail::rootSegment(track, 0.0f, dur, fallback);
        for (long i = 0, n = std::min(cycles - 1, 1024L); i < n; ++i) total = composeRootMotion(total, full);
    }
    return composeRootMotion(total, detail::rootSegment(track, 0.0f, b, fallback));
}

} // namespace anvil
