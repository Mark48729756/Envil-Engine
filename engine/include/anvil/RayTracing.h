#pragma once
#include <cstdint>
#include <vector>
namespace anvil {
struct RtGeometry{std::uint32_t vertexCount=0,indexCount=0;};struct RtInstance{std::uint32_t geometry=0;std::uint64_t instanceId=0;};
class RayTracingSceneModel{public:std::uint32_t addGeometry(RtGeometry g){if(!g.vertexCount||g.indexCount<3||g.indexCount%3)return UINT32_MAX;geometries_.push_back(g);return std::uint32_t(geometries_.size()-1);}bool addInstance(std::uint32_t g,std::uint64_t id){if(g>=geometries_.size()||!id)return false;for(const auto&instance:instances_)if(instance.instanceId==id)return false;instances_.push_back({g,id});return true;}bool ready()const{return !geometries_.empty()&&!instances_.empty();}std::size_t geometryCount()const{return geometries_.size();}std::size_t instanceCount()const{return instances_.size();}private:std::vector<RtGeometry>geometries_;std::vector<RtInstance>instances_;};}
