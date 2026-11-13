#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace anvil {
struct PhysicsParticle{Vec3 position{},previous{};float inverseMass=1;};struct DistanceConstraint{std::uint32_t a=0,b=0;float restLength=1,stiffness=1;};
class ConstraintSolver {
public:
    std::uint32_t addParticle(PhysicsParticle p){if(!finite(p.position)||!finite(p.previous)||!std::isfinite(p.inverseMass)||p.inverseMass<0)return UINT32_MAX;if(particles_.size()>=UINT32_MAX)return UINT32_MAX;particles_.push_back(p);return static_cast<std::uint32_t>(particles_.size()-1);}
    bool addDistance(DistanceConstraint c){if(c.a>=particles_.size()||c.b>=particles_.size()||c.a==c.b||!std::isfinite(c.restLength)||c.restLength<0||!std::isfinite(c.stiffness))return false;c.stiffness=std::clamp(c.stiffness,0.0f,1.0f);constraints_.push_back(c);return true;}
    void step(float dt,Vec3 gravity={0,-9.81f,0},unsigned iterations=5){if(!std::isfinite(dt)||!finite(gravity))return;dt=std::clamp(dt,1e-5f,0.05f);for(auto&p:particles_)if(p.inverseMass>0){Vec3 v{p.position.x-p.previous.x,p.position.y-p.previous.y,p.position.z-p.previous.z};p.previous=p.position;p.position={p.position.x+v.x+gravity.x*dt*dt,p.position.y+v.y+gravity.y*dt*dt,p.position.z+v.z+gravity.z*dt*dt};}for(unsigned n=0;n<std::clamp(iterations,1u,32u);++n)for(const auto&c:constraints_){auto&a=particles_[c.a];auto&b=particles_[c.b];Vec3 d{b.position.x-a.position.x,b.position.y-a.position.y,b.position.z-a.position.z};float len=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(!std::isfinite(len)||len<1e-6f)continue;float wa=a.inverseMass,wb=b.inverseMass,sum=wa+wb;if(sum<=0)continue;float error=(len-c.restLength)/len*c.stiffness;a.position.x+=d.x*error*wa/sum;a.position.y+=d.y*error*wa/sum;a.position.z+=d.z*error*wa/sum;b.position.x-=d.x*error*wb/sum;b.position.y-=d.y*error*wb/sum;b.position.z-=d.z*error*wb/sum;}}
    const std::vector<PhysicsParticle>&particles()const{return particles_;}
private:static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}std::vector<PhysicsParticle>particles_;std::vector<DistanceConstraint>constraints_;
};
} // namespace anvil
