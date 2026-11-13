#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace anvil::ecs {
using Entity = std::uint32_t;
class IStorage { public: virtual ~IStorage()=default; virtual void erase(Entity)=0; virtual bool contains(Entity) const=0; };
template<class T> class Storage final : public IStorage {
public:
    template<class... A> T& emplace(Entity e,A&&... a){ auto [it,inserted]=data.try_emplace(e,std::forward<A>(a)...); if(!inserted) throw std::logic_error("ECS component already exists"); return it->second; }
    T* get(Entity e){auto i=data.find(e);return i==data.end()?nullptr:&i->second;}
    const T* get(Entity e)const{auto i=data.find(e);return i==data.end()?nullptr:&i->second;}
    void erase(Entity e)override{data.erase(e);} bool contains(Entity e)const override{return data.find(e)!=data.end();}
    std::unordered_map<Entity,T> data;
};
// Type-erased sparse component stores; entities are stable integer handles until destroyed.
class Registry {
public:
    Entity create(){Entity e=next_++;if(!e)e=next_++;alive_.push_back(e);aliveSet_.insert(e);return e;}
    // M32: create an entity with a caller-chosen id (Scene uses its persistent entity ids). Returns false if id is 0 or taken.
    bool createWithId(Entity e){if(!e||aliveSet_.count(e))return false;alive_.push_back(e);aliveSet_.insert(e);if(e>=next_)next_=e+1;return true;}
    bool valid(Entity e)const{return aliveSet_.count(e)!=0;}   // O(1); was a linear scan
    void destroy(Entity e){if(!valid(e))return;aliveSet_.erase(e);alive_.erase(std::remove(alive_.begin(),alive_.end(),e),alive_.end());for(auto& p:stores_)p.second->erase(e);}
    template<class T,class... A> T& emplace(Entity e,A&&... a){require(e);return storage<T>().emplace(e,std::forward<A>(a)...);}
    template<class T> T* get(Entity e){auto i=stores_.find(typeid(T));return i==stores_.end()?nullptr:static_cast<Storage<T>*>(i->second.get())->get(e);}
    template<class T> const T* get(Entity e)const{auto i=stores_.find(typeid(T));return i==stores_.end()?nullptr:static_cast<const Storage<T>*>(i->second.get())->get(e);}
    template<class T> bool remove(Entity e){auto i=stores_.find(typeid(T));if(i==stores_.end())return false;auto*s=static_cast<Storage<T>*>(i->second.get());if(!s->contains(e))return false;s->erase(e);return true;}
    template<class... T> std::vector<Entity> view()const{std::vector<Entity> out;for(Entity e:alive_)if((get<T>(e)&&...))out.push_back(e);return out;}
    const std::vector<Entity>& entities()const{return alive_;}
    void clear(){alive_.clear();aliveSet_.clear();stores_.clear();next_=1;}
private:
    template<class T> Storage<T>& storage(){auto key=std::type_index(typeid(T));auto i=stores_.find(key);if(i==stores_.end())i=stores_.emplace(key,std::make_unique<Storage<T>>()).first;return *static_cast<Storage<T>*>(i->second.get());}
    void require(Entity e)const{if(!valid(e))throw std::invalid_argument("ECS entity is not alive");}
    Entity next_=1;std::vector<Entity> alive_;std::unordered_set<Entity> aliveSet_;std::unordered_map<std::type_index,std::unique_ptr<IStorage>> stores_;
};
}
