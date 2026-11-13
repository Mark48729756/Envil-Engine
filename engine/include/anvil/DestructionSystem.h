#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>
namespace anvil {
struct DestructionChunk{std::uint32_t id=0;Vec3 position{},velocity{};float mass=1;bool active=true;};
class DestructionSystem {
public:
    bool fracture(Vec3 center,float radius,std::uint32_t count,std::uint32_t seed=1){if(!finite(center)||!std::isfinite(radius)||radius<=0||count==0)return false;chunks_.clear();std::mt19937 rng(seed);std::uniform_real_distribution<float>d(-1,1);for(std::uint32_t i=0;i<count;++i){Vec3 dir{d(rng),d(rng),d(rng)};float len=std::sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);if(len<1e-4f){dir={0,1,0};len=1;}dir={dir.x/len,dir.y/len,dir.z/len};const float r=radius*std::cbrt(std::generate_canonical<float,16>(rng));chunks_.push_back({i,{center.x+dir.x*r,center.y+dir.y*r,center.z+dir.z*r},{dir.x*2.5f,dir.y*2.5f,dir.z*2.5f},1,true});}return true;}
    void applyImpulse(Vec3 origin,float impulse,float falloffRadius){if(!finite(origin)||!std::isfinite(impulse)||!std::isfinite(falloffRadius)||falloffRadius<=0)return;for(auto&c:chunks_)if(c.active){Vec3 d{c.position.x-origin.x,c.position.y-origin.y,c.position.z-origin.z};float len=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(len<falloffRadius&&len>1e-5f){float k=impulse*(1-len/falloffRadius)/std::max(c.mass,1e-4f);c.velocity.x+=d.x/len*k;c.velocity.y+=d.y/len*k;c.velocity.z+=d.z/len*k;}}}
    void update(float dt,Vec3 gravity={0,-9.81f,0}){if(!std::isfinite(dt)||!finite(gravity)||dt<=0)return;dt=std::min(dt,0.05f);for(auto&c:chunks_)if(c.active){c.velocity.x+=gravity.x*dt;c.velocity.y+=gravity.y*dt;c.velocity.z+=gravity.z*dt;c.position.x+=c.velocity.x*dt;c.position.y+=c.velocity.y*dt;c.position.z+=c.velocity.z*dt;}}
    bool deactivate(std::uint32_t id){if(id>=chunks_.size()||!chunks_[id].active)return false;chunks_[id].active=false;return true;}
    const std::vector<DestructionChunk>&chunks()const{return chunks_;}
private:static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}std::vector<DestructionChunk>chunks_;
};
} // namespace anvil
