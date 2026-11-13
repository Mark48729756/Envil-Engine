#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace anvil {
struct RenderPass { std::string name; std::vector<std::string> reads,writes; };
class RenderGraph {
public:
    bool addPass(RenderPass p) {
        if(p.name.empty())return false;
        for(const auto&x:passes_)if(x.name==p.name)return false;
        passes_.push_back(std::move(p));return true;
    }
    bool compile(std::vector<std::string>&order)const {
        order.clear();
        std::unordered_map<std::string,std::size_t> lastWriter;
        std::unordered_map<std::string,std::vector<std::size_t>> readersSinceWrite;
        std::vector<std::vector<std::size_t>> edges(passes_.size());
        std::vector<std::size_t> indegree(passes_.size(),0);
        for(std::size_t i=0;i<passes_.size();++i){
            std::unordered_set<std::size_t> dependencies;
            for(const auto&r:passes_[i].reads){
                auto w=lastWriter.find(r);if(w!=lastWriter.end()&&w->second!=i)dependencies.insert(w->second);
                readersSinceWrite[r].push_back(i);
            }
            for(const auto&w:passes_[i].writes){
                auto prev=lastWriter.find(w);if(prev!=lastWriter.end()&&prev->second!=i)dependencies.insert(prev->second);
                auto readers=readersSinceWrite.find(w);
                if(readers!=readersSinceWrite.end()){
                    for(auto reader:readers->second)if(reader!=i)dependencies.insert(reader);
                    readers->second.clear();
                }
                lastWriter[w]=i;
            }
            for(auto dep:dependencies){edges[dep].push_back(i);++indegree[i];}
        }
        std::vector<std::size_t> ready;ready.reserve(passes_.size());
        for(std::size_t i=0;i<indegree.size();++i)if(indegree[i]==0)ready.push_back(i);
        for(std::size_t n=0;n<ready.size();++n){const auto i=ready[n];order.push_back(passes_[i].name);for(auto j:edges[i])if(--indegree[j]==0)ready.push_back(j);}
        if(order.size()!=passes_.size()){order.clear();return false;}return true;
    }
private:
    std::vector<RenderPass>passes_;
};
} // namespace anvil
