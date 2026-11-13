#pragma once
#include <climits>
#include <cstdint>
#include <string>
#include <unordered_map>
namespace anvil {
struct GameplaySnapshot { std::unordered_map<std::string,int>inventory;std::unordered_map<std::string,bool>flags; };
class GameplayState {
public:
    bool addItem(const std::string&id,int n=1){if(id.empty()||n<=0)return false;auto i=inventory_.find(id);const int current=i==inventory_.end()?0:i->second;if(current<0||n>INT_MAX-current)return false;inventory_[id]=current+n;return true;}
    bool removeItem(const std::string&id,int n=1){auto i=inventory_.find(id);if(i==inventory_.end()||n<=0||i->second<n)return false;i->second-=n;if(!i->second)inventory_.erase(i);return true;}
    int count(const std::string&id)const{auto i=inventory_.find(id);return i==inventory_.end()?0:i->second;}
    void setFlag(std::string id,bool v){if(!id.empty())flags_[std::move(id)]=v;}
    bool flag(const std::string&id)const{auto i=flags_.find(id);return i!=flags_.end()&&i->second;}
    GameplaySnapshot save()const{return {inventory_,flags_};}
    void load(const GameplaySnapshot&s){inventory_.clear();for(const auto&[id,n]:s.inventory)if(!id.empty()&&n>0)inventory_.emplace(id,n);flags_.clear();for(const auto&[id,v]:s.flags)if(!id.empty())flags_.emplace(id,v);}
private:
    std::unordered_map<std::string,int>inventory_;std::unordered_map<std::string,bool>flags_;
};
} // namespace anvil
