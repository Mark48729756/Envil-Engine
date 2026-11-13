#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace anvil {
struct AudioListener{Vec3 position{},forward{0,0,-1},up{0,1,0};};
struct AudioEmitter{std::uint64_t id=0;Vec3 position{},velocity{};float gain=1,minDistance=1,maxDistance=50,occlusion=0;bool looping=false,playing=false;};
struct SpatialAudioResult{float gain=0,pan=0,lowPassHz=22000,doppler=1;};
class AudioEngine {
public:
    void setListener(AudioListener l){if(finite(l.position)&&finite(l.forward)&&finite(l.up)&&length(l.forward)>1e-6f&&length(cross(l.forward,l.up))>1e-6f)listener_=l;}
    bool addEmitter(AudioEmitter e){if(!e.id||find(e.id)||!finite(e.position)||!finite(e.velocity)||!std::isfinite(e.gain)||e.gain<0||!std::isfinite(e.minDistance)||e.minDistance<0||!std::isfinite(e.maxDistance)||e.maxDistance<=e.minDistance||!std::isfinite(e.occlusion))return false;emitters_.push_back(e);return true;}
    bool removeEmitter(std::uint64_t id){auto i=std::find_if(emitters_.begin(),emitters_.end(),[&](const auto&e){return e.id==id;});if(i==emitters_.end())return false;emitters_.erase(i);return true;}
    AudioEmitter*find(std::uint64_t id){auto i=std::find_if(emitters_.begin(),emitters_.end(),[&](auto&e){return e.id==id;});return i==emitters_.end()?nullptr:&*i;}
    bool setPlaying(std::uint64_t id,bool v){auto*e=find(id);if(!e)return false;e->playing=v;return true;}
    SpatialAudioResult evaluate(std::uint64_t id,float speedOfSound=343.0f)const{
        auto i=std::find_if(emitters_.begin(),emitters_.end(),[&](const auto&e){return e.id==id;});if(i==emitters_.end()||!i->playing||!std::isfinite(speedOfSound)||speedOfSound<=0)return{};
        const Vec3 d{i->position.x-listener_.position.x,i->position.y-listener_.position.y,i->position.z-listener_.position.z};const float dist=length(d);if(!std::isfinite(dist))return{};
        const float range=std::clamp((i->maxDistance-dist)/(i->maxDistance-i->minDistance),0.0f,1.0f),attenuation=dist<=i->minDistance?1.0f:range*range;
        const Vec3 f=normalize(listener_.forward),right=normalize(cross(f,listener_.up));const float pan=dist>1e-5f?std::clamp(dot(d,right)/dist,-1.0f,1.0f):0.0f;
        const float radial=dot(i->velocity,d)/std::max(dist,1e-4f),doppler=std::clamp(speedOfSound/std::max(1.0f,speedOfSound+radial),0.5f,2.0f),occ=std::clamp(i->occlusion,0.0f,1.0f);
        return{i->gain*attenuation*(1-0.75f*occ),pan,22000+(800-22000)*occ,doppler};
    }
    const std::vector<AudioEmitter>&emitters()const{return emitters_;}
private:
    static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}static float length(Vec3 v){return std::sqrt(dot(v,v));}static float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}static Vec3 cross(Vec3 a,Vec3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}static Vec3 normalize(Vec3 v){const float l=length(v);return l>1e-6f?Vec3{v.x/l,v.y/l,v.z/l}:Vec3{0,0,0};}
    AudioListener listener_{};std::vector<AudioEmitter>emitters_;
};
} // namespace anvil
