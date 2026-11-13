#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iterator>
#include <unordered_map>
#include <vector>
namespace anvil {
struct VirtualPageId { std::uint32_t mip=0,x=0,y=0; bool operator==(const VirtualPageId&)const=default; };
struct VirtualPageHash {
    std::size_t operator()(const VirtualPageId&p)const noexcept {
        std::uint64_t h=(static_cast<std::uint64_t>(p.mip)<<42)^(static_cast<std::uint64_t>(p.x)<<21)^p.y;
        h^=h>>30;h*=0xbf58476d1ce4e5b9ULL;h^=h>>27;h*=0x94d049bb133111ebULL;return static_cast<std::size_t>(h^(h>>31));
    }
};
struct VirtualPageRequest { VirtualPageId page;float screenPriority=0.0f; };
struct VirtualPageMapping { VirtualPageId requested,resident;bool exact=false; };
class VirtualTextureCache {
public:
    explicit VirtualTextureCache(std::size_t capacityPages=256):capacity_(std::max<std::size_t>(1,capacityPages)){}
    std::vector<VirtualPageId> processFeedback(std::vector<VirtualPageRequest> requests){
        std::unordered_map<VirtualPageId,float,VirtualPageHash> unique;
        for(auto&r:requests){if(!std::isfinite(r.screenPriority))r.screenPriority=0.0f;auto[it,inserted]=unique.emplace(r.page,r.screenPriority);if(!inserted)it->second=std::max(it->second,r.screenPriority);}
        std::vector<VirtualPageRequest> ordered;ordered.reserve(unique.size());
        for(const auto&[page,priority]:unique)if(!resident_.count(page))ordered.push_back({page,priority});
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto&a,const auto&b){return a.screenPriority>b.screenPriority;});
        std::vector<VirtualPageId> admitted;admitted.reserve(std::min(capacity_,ordered.size()));
        for(const auto&r:ordered){makeRoom();resident_[r.page]=nextStamp();admitted.push_back(r.page);}return admitted;
    }
    bool touch(VirtualPageId page){auto it=resident_.find(page);if(it==resident_.end())return false;it->second=nextStamp();return true;}
    VirtualPageMapping resolve(VirtualPageId page,std::uint32_t maxMip=16)const{
        VirtualPageId candidate=page;
        for(std::uint32_t i=0;i<=maxMip;++i){if(resident_.count(candidate))return {page,candidate,i==0};if(candidate.mip==std::numeric_limits<std::uint32_t>::max())break;candidate={candidate.mip+1,candidate.x/2,candidate.y/2};}
        return {page,candidate,false};
    }
    bool contains(VirtualPageId page)const{return resident_.count(page)!=0;}
    std::size_t residentCount()const{return resident_.size();}std::size_t capacity()const{return capacity_;}
private:
    std::uint64_t nextStamp(){
        if(clock_==std::numeric_limits<std::uint64_t>::max()){
            std::vector<std::pair<VirtualPageId,std::uint64_t>> ordered(resident_.begin(),resident_.end());
            std::sort(ordered.begin(),ordered.end(),[](const auto&a,const auto&b){return a.second<b.second;});
            std::uint64_t stamp=0;for(const auto&entry:ordered)resident_[entry.first]=++stamp;clock_=stamp;
        }
        return ++clock_;
    }
    void makeRoom(){if(resident_.size()<capacity_)return;auto victim=resident_.begin();for(auto it=std::next(resident_.begin());it!=resident_.end();++it)if(it->second<victim->second)victim=it;resident_.erase(victim);}
    std::size_t capacity_;std::uint64_t clock_=0;std::unordered_map<VirtualPageId,std::uint64_t,VirtualPageHash>resident_;
};
} // namespace anvil
