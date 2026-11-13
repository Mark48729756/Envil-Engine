#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace anvil {
struct WaterWave {float amplitude=0.1f,wavelength=4.0f,speed=1.0f,directionX=1.0f,directionZ=0.0f;};
struct WaterSample {float height=0.0f,normalX=0.0f,normalY=1.0f,normalZ=0.0f;};
class WaterSurface {
public:
    explicit WaterSurface(std::vector<WaterWave>waves={{0.12f,5.0f,1.0f,1.0f,0.2f},{0.045f,1.8f,1.7f,-0.3f,1.0f}}){for(const auto&w:waves)if(std::isfinite(w.amplitude)&&std::isfinite(w.wavelength)&&w.wavelength>0&&std::isfinite(w.speed)&&std::isfinite(w.directionX)&&std::isfinite(w.directionZ))waves_.push_back(w);}
    WaterSample sample(float x,float z,float time)const{
        if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(time))return{};
        constexpr float pi=3.14159265358979323846f;float h=0,dx=0,dz=0;
        for(const auto&w:waves_){const float norm=1.0f/std::max(0.0001f,std::hypot(w.directionX,w.directionZ));const float dirX=w.directionX*norm,dirZ=w.directionZ*norm,k=2*pi/w.wavelength,phase=k*(dirX*x+dirZ*z)-w.speed*time,s=std::sin(phase),c=std::cos(phase);h+=w.amplitude*s;dx+=w.amplitude*k*dirX*c;dz+=w.amplitude*k*dirZ*c;}
        const float inv=1.0f/std::sqrt(dx*dx+1.0f+dz*dz);return{h,-dx*inv,inv,-dz*inv};
    }
    static float fresnelSchlick(float cosTheta,float f0=0.02f){if(!std::isfinite(cosTheta)||!std::isfinite(f0))return 0;const float x=1-std::clamp(cosTheta,0.0f,1.0f),x2=x*x;return std::clamp(f0,0.0f,1.0f)+(1-std::clamp(f0,0.0f,1.0f))*x2*x2*x;}
    static float shoreFoam(float depth,float width=0.6f){if(!std::isfinite(depth)||!std::isfinite(width))return 0;const float w=std::max(0.0001f,width);return 1-std::clamp(depth/w,0.0f,1.0f);}
private:std::vector<WaterWave>waves_;
};
} // namespace anvil
