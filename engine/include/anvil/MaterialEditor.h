#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
namespace anvil {
struct MaterialParameter { std::string name;float value=0,min=0,max=1; };
struct MaterialLayerDef { std::string name;float weight=1; };
class MaterialEditorModel {
public:
    bool set(const std::string&n,float v,float lo=0,float hi=1){if(n.empty()||!std::isfinite(v)||!std::isfinite(lo)||!std::isfinite(hi)||lo>hi)return false;for(auto&p:params_)if(p.name==n){p.value=std::clamp(v,lo,hi);p.min=lo;p.max=hi;return true;}params_.push_back({n,std::clamp(v,lo,hi),lo,hi});return true;}
    float get(const std::string&n,float fallback=0)const{for(const auto&p:params_)if(p.name==n)return p.value;return fallback;}
    bool addLayer(std::string n,float w){if(n.empty()||!std::isfinite(w))return false;layers_.push_back({std::move(n),std::clamp(w,0.0f,1.0f)});return true;}
    const auto&parameters()const{return params_;}const auto&layers()const{return layers_;}
private:std::vector<MaterialParameter>params_;std::vector<MaterialLayerDef>layers_;
};
} // namespace anvil
