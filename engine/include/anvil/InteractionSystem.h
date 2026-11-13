#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace anvil {
struct Interactable{std::uint64_t id=0;Vec3 position{};float radius=1.5f;std::string prompt;bool enabled=true;};
class InteractionSystem {
public:
    bool add(Interactable v){if(!v.id||find(v.id)||!finite(v.position)||!std::isfinite(v.radius)||v.radius<=0)return false;items_.push_back(std::move(v));return true;}
    Interactable*find(std::uint64_t id){auto i=std::find_if(items_.begin(),items_.end(),[&](auto&v){return v.id==id;});return i==items_.end()?nullptr:&*i;}
    const Interactable*find(std::uint64_t id)const{auto i=std::find_if(items_.begin(),items_.end(),[&](auto&v){return v.id==id;});return i==items_.end()?nullptr:&*i;}
    std::uint64_t nearest(Vec3 p,float maxDistance)const{if(!finite(p)||!std::isfinite(maxDistance)||maxDistance<0)return 0;const Interactable*best=nullptr;float bestD=maxDistance;for(const auto&v:items_)if(v.enabled){const float x=v.position.x-p.x,y=v.position.y-p.y,z=v.position.z-p.z,d=std::sqrt(x*x+y*y+z*z);if(d<=std::min(maxDistance,v.radius)&&(!best||d<bestD)){best=&v;bestD=d;}}return best?best->id:0;}
    bool interact(std::uint64_t id){auto*v=find(id);if(!v||!v->enabled)return false;lastInteracted_=id;return true;}
    std::uint64_t lastInteracted()const{return lastInteracted_;}const std::vector<Interactable>&items()const{return items_;}
private:static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}std::vector<Interactable>items_;std::uint64_t lastInteracted_=0;
};
} // namespace anvil
