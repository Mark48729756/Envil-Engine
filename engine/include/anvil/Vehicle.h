#pragma once
#include <algorithm>
#include <cmath>
namespace anvil {
struct VehicleTuning { float mass=1200,engineForce=6000,brakeForce=9000,maxSteerRadians=0.55f,drag=0.35f; };
class VehicleModel {
public:
    explicit VehicleModel(VehicleTuning t={}) : t_(t) {}
    bool valid() const { return std::isfinite(t_.mass)&&t_.mass>0&&std::isfinite(t_.engineForce)&&t_.engineForce>=0&&std::isfinite(t_.brakeForce)&&t_.brakeForce>=0&&std::isfinite(t_.maxSteerRadians)&&t_.maxSteerRadians>=0&&std::isfinite(t_.drag)&&t_.drag>=0; }
    void step(float throttle,float brake,float steer,float dt) {
        if(!valid()||!std::isfinite(throttle)||!std::isfinite(brake)||!std::isfinite(steer)||!std::isfinite(dt)||dt<=0||dt>0.1f)return;
        throttle=std::clamp(throttle,-1.0f,1.0f);brake=std::clamp(brake,0.0f,1.0f);steer=std::clamp(steer,-1.0f,1.0f);
        const float driveAndDrag=throttle*t_.engineForce-t_.drag*speed_*std::abs(speed_);
        float next=speed_+driveAndDrag/t_.mass*dt;
        const float brakingDelta=t_.brakeForce*brake/t_.mass*dt;
        if(next>0.0f)next=std::max(0.0f,next-brakingDelta);
        else if(next<0.0f)next=std::min(0.0f,next+brakingDelta);
        speed_=next;steering_=steer*t_.maxSteerRadians;
    }
    float speed()const{return speed_;}float steeringAngle()const{return steering_;}void reset(){speed_=steering_=0;}
private:
    VehicleTuning t_;float speed_=0,steering_=0;
};
} // namespace anvil
