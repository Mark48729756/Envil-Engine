#include "anvil/physics/PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace anvil::physics {
PhysicsWorld::PhysicsWorld(glm::vec3 gravity):gravity_(gravity) {
    if (!std::isfinite(gravity.x) || !std::isfinite(gravity.y) || !std::isfinite(gravity.z))
        throw std::invalid_argument("Physics gravity must be finite");
}
void PhysicsWorld::setGravity(glm::vec3 value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
        throw std::invalid_argument("Physics gravity must be finite");
    gravity_ = value;
}
std::uint32_t PhysicsWorld::createBody(const BodyDesc& input){
    BodyDesc d=input;
    d.halfExtents=glm::max(glm::abs(d.halfExtents),glm::vec3(0.001f));
    d.mass=std::max(std::isfinite(d.mass)?d.mass:1.0f,0.0001f);
    d.restitution=glm::clamp(std::isfinite(d.restitution)?d.restitution:0.05f,0.0f,1.0f);
    d.friction=glm::clamp(std::isfinite(d.friction)?d.friction:0.65f,0.0f,1.0f);
    d.linearDamping=glm::clamp(std::isfinite(d.linearDamping)?d.linearDamping:0.02f,0.0f,20.0f);
    d.gravityScale=std::clamp(std::isfinite(d.gravityScale)?d.gravityScale:1.0f,-10.0f,10.0f);
    if (!std::isfinite(d.position.x)||!std::isfinite(d.position.y)||!std::isfinite(d.position.z)) d.position=glm::vec3(0.0f);
    if (!std::isfinite(d.velocity.x)||!std::isfinite(d.velocity.y)||!std::isfinite(d.velocity.z)) d.velocity=glm::vec3(0.0f);
    const auto id=nextId_++; bodies_.push_back({id,d}); return id;
}
bool PhysicsWorld::destroyBody(std::uint32_t id){auto i=std::find_if(bodies_.begin(),bodies_.end(),[&](const Body& b){return b.id==id;});if(i==bodies_.end())return false;bodies_.erase(i);return true;}
Body* PhysicsWorld::getBody(std::uint32_t id){for(auto& b:bodies_)if(b.id==id)return &b;return nullptr;}
const Body* PhysicsWorld::getBody(std::uint32_t id)const{for(const auto& b:bodies_)if(b.id==id)return &b;return nullptr;}
void PhysicsWorld::setFixedDelta(float v){if(!(v>0.0f)||!std::isfinite(v))throw std::invalid_argument("Physics fixed delta must be finite and positive");fixedDelta_=v;accumulator_=std::min(accumulator_,fixedDelta_);}
void PhysicsWorld::step(float frameDelta){if(!std::isfinite(frameDelta)||frameDelta<=0.0f)return;accumulator_+=std::min(frameDelta,0.25f);unsigned n=0;while(accumulator_>=fixedDelta_&&n<maxSubsteps){simulate(fixedDelta_);accumulator_-=fixedDelta_;++n;}if(n==maxSubsteps&&accumulator_>=fixedDelta_)accumulator_=std::fmod(accumulator_,fixedDelta_);}
void PhysicsWorld::simulate(float dt){
    for(auto& b:bodies_) if(!b.desc.isStatic) {
        if(b.desc.useGravity) b.desc.velocity += gravity_ * (b.desc.gravityScale * dt);
        // Frame-rate-independent exponential damping.
        b.desc.velocity *= std::exp(-b.desc.linearDamping * dt);
        b.desc.position += b.desc.velocity * dt;
    }
    contacts_.clear();
    for(std::size_t i=0;i<bodies_.size();++i)for(std::size_t j=i+1;j<bodies_.size();++j){auto& a=bodies_[i];auto& b=bodies_[j];if(a.desc.isStatic&&b.desc.isStatic)continue;
        const glm::vec3 d=b.desc.position-a.desc.position;const glm::vec3 overlap=a.desc.halfExtents+b.desc.halfExtents-glm::abs(d);
        if(overlap.x<=0||overlap.y<=0||overlap.z<=0)continue;
        int axis=overlap.y<overlap.x?1:0;if(overlap.z<overlap[axis])axis=2;
        glm::vec3 normal(0.0f);normal[axis]=d[axis]>=0.0f?1.0f:-1.0f;const float penetration=overlap[axis];contacts_.push_back({a.id,b.id,normal,penetration});
        const float invA=a.desc.isStatic?0.0f:1.0f/a.desc.mass,invB=b.desc.isStatic?0.0f:1.0f/b.desc.mass,invSum=invA+invB;if(invSum<=0)continue;
        const glm::vec3 correction=normal*(std::max(penetration-0.001f,0.0f)*0.8f/invSum);a.desc.position-=correction*invA;b.desc.position+=correction*invB;
        const glm::vec3 rv=b.desc.velocity-a.desc.velocity;const float vn=glm::dot(rv,normal);if(vn<0.0f){const float e=std::min(a.desc.restitution,b.desc.restitution);const float impulse=-(1.0f+e)*vn/invSum;const glm::vec3 J=normal*impulse;a.desc.velocity-=J*invA;b.desc.velocity+=J*invB;
            glm::vec3 tangent=rv-normal*vn;const float tl=glm::length(tangent);if(tl>1e-6f){tangent/=tl;float jt=-glm::dot(rv,tangent)/invSum;const float mu=std::sqrt(a.desc.friction*b.desc.friction);jt=glm::clamp(jt,-impulse*mu,impulse*mu);const glm::vec3 f=tangent*jt;a.desc.velocity-=f*invA;b.desc.velocity+=f*invB;}}
    }
}
}
