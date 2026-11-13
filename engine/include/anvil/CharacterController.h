#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cmath>
namespace anvil {
struct CharacterMoveInput{float moveX=0,moveZ=0;bool jumpPressed=false;};
struct CharacterControllerConfig{float radius=0.35f,height=1.8f,moveSpeed=5,acceleration=24,gravity=-24,jumpSpeed=7.5f,maxSlopeDegrees=48;};
struct CharacterControllerState{Vec3 position{},velocity{};bool grounded=false,jumping=false;};
class CharacterController {
public:
    explicit CharacterController(CharacterControllerConfig c={}):config_(c){
        if(!std::isfinite(config_.radius)||config_.radius<=0)config_.radius=0.35f;
        if(!std::isfinite(config_.height)||config_.height<=0)config_.height=1.8f;
        if(!std::isfinite(config_.moveSpeed)||config_.moveSpeed<0)config_.moveSpeed=0;
        if(!std::isfinite(config_.acceleration)||config_.acceleration<0)config_.acceleration=0;
        if(!std::isfinite(config_.gravity))config_.gravity=-24;
        if(!std::isfinite(config_.jumpSpeed)||config_.jumpSpeed<0)config_.jumpSpeed=0;
        if(!std::isfinite(config_.maxSlopeDegrees))config_.maxSlopeDegrees=48;
        config_.height=std::max(config_.radius*2,config_.height);config_.maxSlopeDegrees=std::clamp(config_.maxSlopeDegrees,0.0f,89.0f);
    }
    void setPosition(Vec3 p){if(finite(p))state_.position=p;}
    const CharacterControllerState&state()const{return state_;}const CharacterControllerConfig&config()const{return config_;}
    void setGroundContact(bool grounded,Vec3 normal={0,1,0}){if(!finite(normal)){state_.grounded=false;return;}const float len=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);if(len<=1e-6f){state_.grounded=false;return;}normal={normal.x/len,normal.y/len,normal.z/len};const float minY=std::cos(config_.maxSlopeDegrees*0.017453292519943295f);state_.grounded=grounded&&normal.y>=minY;if(state_.grounded&&state_.velocity.y<0)state_.velocity.y=0;}
    void update(CharacterMoveInput input,float dt){if(!std::isfinite(dt)||dt<=0||!std::isfinite(input.moveX)||!std::isfinite(input.moveZ))return;dt=std::min(dt,0.1f);float x=input.moveX,z=input.moveZ;const float m=std::hypot(x,z);if(m>1){x/=m;z/=m;}const float targetX=x*config_.moveSpeed,targetZ=z*config_.moveSpeed,maxDelta=config_.acceleration*dt;state_.velocity.x+=std::clamp(targetX-state_.velocity.x,-maxDelta,maxDelta);state_.velocity.z+=std::clamp(targetZ-state_.velocity.z,-maxDelta,maxDelta);if(input.jumpPressed&&state_.grounded){state_.velocity.y=config_.jumpSpeed;state_.grounded=false;state_.jumping=true;}if(!state_.grounded)state_.velocity.y+=config_.gravity*dt;state_.position.x+=state_.velocity.x*dt;state_.position.y+=state_.velocity.y*dt;state_.position.z+=state_.velocity.z*dt;}
private:
    static bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    CharacterControllerConfig config_;CharacterControllerState state_{};
};
} // namespace anvil
