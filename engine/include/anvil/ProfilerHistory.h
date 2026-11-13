#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>
namespace anvil {
class ProfilerHistory {
public:
    explicit ProfilerHistory(std::size_t cap=120):cap_(std::max<std::size_t>(1,cap)){}
    void push(float ms){if(!std::isfinite(ms)||ms<0)return;if(v_.size()==cap_)v_.erase(v_.begin());v_.push_back(ms);}
    float average()const{if(v_.empty())return 0;const double sum=std::accumulate(v_.begin(),v_.end(),0.0);return static_cast<float>(sum/static_cast<double>(v_.size()));}
    float percentile(float p)const{if(v_.empty()||!std::isfinite(p))return 0;auto s=v_;std::sort(s.begin(),s.end());p=std::clamp(p,0.0f,1.0f);return s[static_cast<std::size_t>(p*static_cast<float>(s.size()-1))];}
    std::size_t size()const{return v_.size();}
private:std::size_t cap_;std::vector<float>v_;
};
} // namespace anvil
