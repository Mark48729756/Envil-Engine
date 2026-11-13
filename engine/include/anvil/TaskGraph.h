#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace anvil {
struct TaskNode{std::uint32_t id=0;std::vector<std::uint32_t>dependsOn;};
class TaskGraph {
public:
    bool add(std::uint32_t id){if(!id||nodes_.count(id))return false;nodes_[id]={id,{}};order_.push_back(id);return true;}
    bool depends(std::uint32_t task,std::uint32_t dependency){if(!nodes_.count(task)||!nodes_.count(dependency)||task==dependency)return false;auto&d=nodes_[task].dependsOn;for(auto x:d)if(x==dependency)return false;d.push_back(dependency);if(!orderValid()){d.pop_back();return false;}return true;}
    bool order(std::vector<std::uint32_t>&out)const{
        out.clear();std::unordered_map<std::uint32_t,std::size_t>indegree;std::unordered_map<std::uint32_t,std::vector<std::uint32_t>>outgoing;
        for(auto id:order_)indegree[id]=0;
        for(const auto&[task,node]:nodes_)for(auto dep:node.dependsOn){outgoing[dep].push_back(task);++indegree[task];}
        std::vector<std::uint32_t>ready;ready.reserve(order_.size());for(auto id:order_)if(indegree[id]==0)ready.push_back(id);
        for(std::size_t i=0;i<ready.size();++i){const auto id=ready[i];out.push_back(id);for(auto next:outgoing[id])if(--indegree[next]==0)ready.push_back(next);}
        if(out.size()!=nodes_.size()){out.clear();return false;}return true;
    }
private:bool orderValid()const{std::vector<std::uint32_t>out;return order(out);}std::unordered_map<std::uint32_t,TaskNode>nodes_;std::vector<std::uint32_t>order_;
};
} // namespace anvil
