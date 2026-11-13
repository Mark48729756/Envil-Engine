#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace anvil {
struct RagdollBody{Vec3 position{};float inverseMass=1;bool active=true;};struct RagdollJoint{std::size_t a=0,b=0;float restLength=0,stiffness=0.8f;};
class Ragdoll {
public:
    explicit Ragdoll(std::vector<RagdollBody>bodies={}){for(const auto&b:bodies)if(finite(b.position)&&std::isfinite(b.inverseMass)&&b.inverseMass>=0)bodies_.push_back(b);}
    std::size_t addBody(RagdollBody b){if(!finite(b.position)||!std::isfinite(b.inverseMass)||b.inverseMass<0)return static_cast<std::size_t>(-1);bodies_.push_back(b);return bodies_.size()-1;}
    bool addJoint(std::size_t a,std::size_t b,float restLength,float stiffness=0.8f){if(a>=bodies_.size()||b>=bodies_.size()||a==b||!std::isfinite(restLength)||restLength<0||!std::isfinite(stiffness))return false;joints_.push_back({a,b,restLength,std::clamp(stiffness,0.0f,1.0f)});return true;}
    void setActive(bool active){active_=active;for(auto&b:bodies_)b.active=active;}
    bool active()const{return active_;}void setBlend(float value){if(std::isfinite(value))blend_=std::clamp(value,0.0f,1.0f);}float blend()const{return blend_;}
    void solve(unsigned iterations=4){if(!active_)return;iterations=std::clamp(iterations,1u,32u);for(unsigned it=0;it<iterations;++it)for(const auto&j:joints_){auto&a=bodies_[j.a];auto&b=bodies_[j.b];const float dx=b.position.x-a.position.x,dy=b.position.y-a.position.y,dz=b.position.z-a.position.z,d=std::sqrt(dx*dx+dy*dy+dz*dz);if(!std::isfinite(d)||d<1e-6f)continue;const float wa=a.inverseMass,wb=b.inverseMass,sum=wa+wb;if(sum<1e-6f)continue;const float correction=(d-j.restLength)/d*j.stiffness,ca=correction*wa/sum,cb=correction*wb/sum;a.position.x+=dx*ca;a.position.y+=dy*ca;a.position.z+=dz*ca;b.position.x-=dx*cb;b.position.y-=dy*cb;b.position.z-=dz*cb;}}
    const std::vector<RagdollBody>&bodies()const{return bodies_;}
private:static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}std::vector<RagdollBody>bodies_;std::vector<RagdollJoint>joints_;bool active_=false;float blend_=0;
};
} // namespace anvil
