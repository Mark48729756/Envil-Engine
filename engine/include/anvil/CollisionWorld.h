#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace anvil {
struct Aabb{Vec3 min{},max{};};struct CollisionBody{std::uint64_t id=0;Aabb bounds{};bool isTrigger=false,enabled=true;};struct CollisionPair{std::uint64_t a=0,b=0;};
inline bool validAabb(const Aabb&a){return std::isfinite(a.min.x)&&std::isfinite(a.min.y)&&std::isfinite(a.min.z)&&std::isfinite(a.max.x)&&std::isfinite(a.max.y)&&std::isfinite(a.max.z)&&a.min.x<=a.max.x&&a.min.y<=a.max.y&&a.min.z<=a.max.z;}
inline bool overlaps(const Aabb&a,const Aabb&b){return validAabb(a)&&validAabb(b)&&a.min.x<=b.max.x&&a.max.x>=b.min.x&&a.min.y<=b.max.y&&a.max.y>=b.min.y&&a.min.z<=b.max.z&&a.max.z>=b.min.z;}
class CollisionWorld {
public:
    bool add(CollisionBody b){if(!b.id||find(b.id)||!validAabb(b.bounds))return false;bodies_.push_back(b);return true;}
    CollisionBody*find(std::uint64_t id){auto i=std::find_if(bodies_.begin(),bodies_.end(),[&](auto&b){return b.id==id;});return i==bodies_.end()?nullptr:&*i;}
    bool remove(std::uint64_t id){auto i=std::find_if(bodies_.begin(),bodies_.end(),[&](auto&b){return b.id==id;});if(i==bodies_.end())return false;bodies_.erase(i);return true;}
    std::vector<CollisionPair>detect()const{std::vector<CollisionPair>out;for(std::size_t i=0;i<bodies_.size();++i)if(bodies_[i].enabled)for(std::size_t j=i+1;j<bodies_.size();++j)if(bodies_[j].enabled&&overlaps(bodies_[i].bounds,bodies_[j].bounds))out.push_back({bodies_[i].id,bodies_[j].id});return out;}
    std::vector<std::uint64_t>query(const Aabb&area)const{std::vector<std::uint64_t>out;if(!validAabb(area))return out;for(const auto&b:bodies_)if(b.enabled&&overlaps(b.bounds,area))out.push_back(b.id);return out;}
    const std::vector<CollisionBody>&bodies()const{return bodies_;}
private:std::vector<CollisionBody>bodies_;
};
} // namespace anvil
