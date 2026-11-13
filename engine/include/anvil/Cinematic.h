#pragma once
#include <algorithm>
#include <cmath>
namespace anvil {
struct CinematicSettings {
    float exposure=0,aperture=2.8f,focalLengthMm=50,focusDistance=5,shutterSeconds=1.0f/60.0f;
    bool motionBlur=false,depthOfField=true;
    bool valid()const{return std::isfinite(exposure)&&std::isfinite(aperture)&&aperture>0&&std::isfinite(focalLengthMm)&&focalLengthMm>0&&std::isfinite(focusDistance)&&focusDistance>0&&std::isfinite(shutterSeconds)&&shutterSeconds>0;}
};
inline float circleOfConfusion(float depth,const CinematicSettings&s){
    if(!s.valid()||!s.depthOfField||!std::isfinite(depth)||depth<=0)return 0;
    const float f=s.focalLengthMm/1000.0f,focus=s.focusDistance,denom=s.aperture*depth*(focus-f);
    if(!std::isfinite(denom)||std::abs(denom)<1e-8f)return 0;
    const float coc=std::abs(f*f*(depth-focus)/denom);
    return std::isfinite(coc)?coc:0.0f;
}
} // namespace anvil
