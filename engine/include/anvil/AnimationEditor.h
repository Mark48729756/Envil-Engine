#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace anvil {
struct FloatKey { float time=0,value=0; };
class FloatAnimationTrack {
public:
    bool insert(float t,float v){if(!std::isfinite(t)||!std::isfinite(v)||t<0)return false;auto i=std::lower_bound(keys_.begin(),keys_.end(),t,[](const auto&k,float x){return k.time<x;});if(i!=keys_.end()&&std::abs(i->time-t)<1e-6f)i->value=v;else keys_.insert(i,{t,v});return true;}
    float sample(float t)const{if(keys_.empty()||!std::isfinite(t))return 0;if(t<=keys_.front().time)return keys_.front().value;if(t>=keys_.back().time)return keys_.back().value;auto i=std::upper_bound(keys_.begin(),keys_.end(),t,[](float x,const auto&k){return x<k.time;});if(i==keys_.begin()||i==keys_.end())return 0;const auto&a=*(i-1);const auto&b=*i;const float span=b.time-a.time;if(span<=0)return a.value;const float u=(t-a.time)/span;return a.value+(b.value-a.value)*u;}
    const auto&keys()const{return keys_;}
private:std::vector<FloatKey>keys_;
};
} // namespace anvil
