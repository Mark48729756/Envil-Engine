#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace anvil {

struct Vec3 { float x=0,y=0,z=0; };
struct Quaternion { float x=0,y=0,z=0,w=1; };
struct BoneTransform { Vec3 translation{}; Quaternion rotation{}; Vec3 scale{1,1,1}; };
struct Bone { std::string name; int parent=-1; BoneTransform bindPose{}; };
struct AnimationKey { float time=0.0f; BoneTransform value{}; };
struct BoneTrack { std::vector<AnimationKey> keys; };
struct AnimationEvent { float time=0.0f; std::string name; };
struct AnimationClip { std::string name; float duration=0.0f; std::vector<BoneTrack> tracks; std::vector<AnimationEvent> events; };

inline Quaternion normalize(Quaternion q) {
    const float l=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return l>1e-7f ? Quaternion{q.x/l,q.y/l,q.z/l,q.w/l} : Quaternion{};
}
inline Quaternion slerp(Quaternion a, Quaternion b, float t) {
    a=normalize(a); b=normalize(b);
    float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(d<0.0f){b={-b.x,-b.y,-b.z,-b.w};d=-d;}
    if(d>0.9995f) return normalize({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t});
    const float theta=std::acos(std::clamp(d,-1.0f,1.0f)), sn=std::sin(theta);
    const float wa=std::sin((1.0f-t)*theta)/sn, wb=std::sin(t*theta)/sn;
    return normalize({a.x*wa+b.x*wb,a.y*wa+b.y*wb,a.z*wa+b.z*wb,a.w*wa+b.w*wb});
}
inline BoneTransform blend(BoneTransform a, BoneTransform b, float t) {
    t=std::clamp(t,0.0f,1.0f);
    return {{a.translation.x+(b.translation.x-a.translation.x)*t,a.translation.y+(b.translation.y-a.translation.y)*t,a.translation.z+(b.translation.z-a.translation.z)*t},
        slerp(a.rotation,b.rotation,t),{a.scale.x+(b.scale.x-a.scale.x)*t,a.scale.y+(b.scale.y-a.scale.y)*t,a.scale.z+(b.scale.z-a.scale.z)*t}};
}

class Skeleton {
public:
    explicit Skeleton(std::vector<Bone> bones={}) : bones_(std::move(bones)) {
        for(std::size_t i=0;i<bones_.size();++i) if(bones_[i].parent>=static_cast<int>(i) || bones_[i].parent < -1) throw std::invalid_argument("skeleton parents must precede children");
    }
    const std::vector<Bone>& bones() const { return bones_; }
    std::size_t size() const { return bones_.size(); }
private: std::vector<Bone> bones_;
};

inline BoneTransform sampleTrack(const BoneTrack& track, float time, const BoneTransform& fallback={}) {
    if(track.keys.empty()) return fallback;
    if(time<=track.keys.front().time) return track.keys.front().value;
    if(time>=track.keys.back().time) return track.keys.back().value;
    auto upper=std::upper_bound(track.keys.begin(),track.keys.end(),time,[](float t,const AnimationKey& k){return t<k.time;});
    const auto& b=*upper; const auto& a=*(upper-1);
    const float span=std::max(1e-6f,b.time-a.time);
    return blend(a.value,b.value,(time-a.time)/span);
}

class Animator {
public:
    explicit Animator(const Skeleton& skeleton) : skeleton_(skeleton), pose_(skeleton.size()) { resetToBindPose(); }
    void resetToBindPose(){for(std::size_t i=0;i<skeleton_.size();++i)pose_[i]=skeleton_.bones()[i].bindPose;}
    bool play(const AnimationClip& clip,float time,bool loop=true){
        if(clip.tracks.size()>skeleton_.size() || clip.duration<0.0f) return false;
        clip_=&clip; time_=time; loop_=loop; fadeDuration_=fadeElapsed_=0.0f; evaluate(); return true;
    }
    bool crossFade(const AnimationClip& clip,float fadeSeconds,bool loop=true){
        if(clip.tracks.size()>skeleton_.size() || clip.duration<0.0f) return false;
        sourcePose_=pose_; clip_=&clip; time_=0.0f; loop_=loop;
        fadeDuration_=std::max(0.0f,fadeSeconds); fadeElapsed_=0.0f; evaluate(); return true;
    }
    void update(float dt){if(!clip_)return;const float step=std::max(0.0f,dt);time_+=step;fadeElapsed_=std::min(fadeDuration_,fadeElapsed_+step);evaluate();}
    const std::vector<BoneTransform>& pose() const{return pose_;}
private:
    void evaluate(){
        if(!clip_)return;
        float t=time_;
        if(loop_ && clip_->duration>1e-6f){t=std::fmod(t,clip_->duration);if(t<0)t+=clip_->duration;}
        else t=std::clamp(t,0.0f,clip_->duration);
        for(std::size_t i=0;i<pose_.size();++i) {
            const BoneTransform target=i<clip_->tracks.size()?sampleTrack(clip_->tracks[i],t,skeleton_.bones()[i].bindPose):skeleton_.bones()[i].bindPose;
            pose_[i]=(fadeDuration_>0.0f && i<sourcePose_.size())?blend(sourcePose_[i],target,fadeElapsed_/fadeDuration_):target;
        }
        if(fadeDuration_>0.0f && fadeElapsed_>=fadeDuration_) { fadeDuration_=0.0f; sourcePose_.clear(); }
    }
    const Skeleton& skeleton_; std::vector<BoneTransform> pose_,sourcePose_; const AnimationClip* clip_=nullptr; float time_=0.0f,fadeDuration_=0.0f,fadeElapsed_=0.0f; bool loop_=true;
};

// Two-bone analytic IK for a planar chain; returns the elbow position.
inline Vec3 solveTwoBoneIK(Vec3 root, Vec3 target, float upperLength, float lowerLength, bool bendPositive=true) {
    const float dx=target.x-root.x, dy=target.y-root.y;
    const float raw=std::sqrt(dx*dx+dy*dy);
    const float d=std::clamp(raw,1e-5f,std::max(1e-5f,upperLength+lowerLength-1e-5f));
    const float a=std::max(1e-5f,upperLength), b=std::max(1e-5f,lowerLength);
    const float along=(a*a-b*b+d*d)/(2.0f*d);
    const float height=std::sqrt(std::max(0.0f,a*a-along*along));
    const float ux=dx/std::max(raw,1e-5f), uy=dy/std::max(raw,1e-5f);
    const float sign=bendPositive?1.0f:-1.0f;
    return {root.x+ux*along-uy*height*sign,root.y+uy*along+ux*height*sign,root.z};
}

} // namespace anvil
