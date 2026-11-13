#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace anvil {
struct GraphNode{std::uint64_t id=0;};struct GraphLink{std::uint64_t from=0,to=0;};
class NodeGraph {
public:
    bool addNode(std::uint64_t id){if(!id||nodes_.count(id))return false;nodes_.emplace(id,true);nodeOrder_.push_back(id);return true;}
    bool connect(std::uint64_t a,std::uint64_t b){if(!nodes_.count(a)||!nodes_.count(b)||a==b)return false;for(const auto&e:links_)if(e.from==a&&e.to==b)return false;links_.push_back({a,b});if(hasCycle()){links_.pop_back();return false;}return true;}
    bool hasCycle()const{
        std::unordered_map<std::uint64_t,std::size_t> indegree;std::unordered_map<std::uint64_t,std::vector<std::uint64_t>> outgoing;
        for(auto id:nodeOrder_)indegree[id]=0;
        for(const auto&e:links_){outgoing[e.from].push_back(e.to);++indegree[e.to];}
        std::vector<std::uint64_t>ready;ready.reserve(nodeOrder_.size());for(auto id:nodeOrder_)if(indegree[id]==0)ready.push_back(id);
        std::size_t visited=0;for(std::size_t i=0;i<ready.size();++i){++visited;for(auto to:outgoing[ready[i]])if(--indegree[to]==0)ready.push_back(to);}return visited!=nodes_.size();
    }
    const std::vector<GraphLink>&links()const{return links_;}std::size_t nodeCount()const{return nodes_.size();}
private:std::unordered_map<std::uint64_t,bool>nodes_;std::vector<std::uint64_t>nodeOrder_;std::vector<GraphLink>links_;
};
} // namespace anvil
