#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <stdexcept>
#include <vector>
namespace anvil {
struct TerrainSample{float height=0,normalX=0,normalY=1,normalZ=0;};struct TerrainLod{std::uint32_t level=0;float morph=0;};
class HeightField {
public:
    HeightField(std::uint32_t width,std::uint32_t height,std::vector<float>samples,float spacing=1.0f)
        :width_(width),height_(height),samples_(std::move(samples)),spacing_(std::isfinite(spacing)&&spacing>0?spacing:0.0001f){
        const auto w=static_cast<std::size_t>(width_),h=static_cast<std::size_t>(height_);
        if(width_<2||height_<2||w>std::numeric_limits<std::size_t>::max()/h||samples_.size()!=w*h)throw std::invalid_argument("HeightField requires valid dimensions and matching data");
        if(!std::all_of(samples_.begin(),samples_.end(),[](float v){return std::isfinite(v);}))throw std::invalid_argument("HeightField samples must be finite");
    }
    float sample(float x,float z)const{
        if(!std::isfinite(x)||!std::isfinite(z))return 0;
        const float gx=std::clamp(x/spacing_,0.0f,static_cast<float>(width_-1)),gz=std::clamp(z/spacing_,0.0f,static_cast<float>(height_-1));
        const auto x0=static_cast<std::uint32_t>(gx),z0=static_cast<std::uint32_t>(gz),x1=std::min(x0+1,width_-1),z1=std::min(z0+1,height_-1);
        const float tx=gx-static_cast<float>(x0),tz=gz-static_cast<float>(z0),a=lerp(at(x0,z0),at(x1,z0),tx),b=lerp(at(x0,z1),at(x1,z1),tx);return lerp(a,b,tz);
    }
    TerrainSample sampleWithNormal(float x,float z)const{if(!std::isfinite(x)||!std::isfinite(z))return{};const float e=spacing_,dx=(sample(x+e,z)-sample(x-e,z))/(2*e),dz=(sample(x,z+e)-sample(x,z-e))/(2*e),inv=1/std::sqrt(dx*dx+1+dz*dz);return{sample(x,z),-dx*inv,inv,-dz*inv};}
    std::uint32_t width()const{return width_;}std::uint32_t height()const{return height_;}float spacing()const{return spacing_;}
private:
    static float lerp(float a,float b,float t){return a+(b-a)*t;}float at(std::uint32_t x,std::uint32_t z)const{return samples_[static_cast<std::size_t>(z)*width_+x];}
    std::uint32_t width_,height_;std::vector<float>samples_;float spacing_;
};
inline TerrainLod selectTerrainLod(float distance,const std::vector<float>&enterDistances,float morphBand=20.0f){
    if(enterDistances.empty()||!std::isfinite(distance)||!std::isfinite(morphBand)||morphBand<=0)return{};
    if(!std::is_sorted(enterDistances.begin(),enterDistances.end())||!std::all_of(enterDistances.begin(),enterDistances.end(),[](float d){return std::isfinite(d)&&d>=0;}))return{};
    const float d=std::max(0.0f,distance);std::size_t level=0;for(std::size_t i=1;i<enterDistances.size();++i){if(d>=enterDistances[i])level=i;else break;}
    float morph=0;if(level+1<enterDistances.size())morph=std::clamp((d-(enterDistances[level+1]-morphBand))/morphBand,0.0f,1.0f);return{static_cast<std::uint32_t>(level),morph};
}
} // namespace anvil
