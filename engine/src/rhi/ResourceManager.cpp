#include "anvil/rhi/ResourceManager.h"
#include <algorithm>
namespace anvil::rhi {
bool ResourceManager::release(ResourceId id) { std::lock_guard lock(mutex_); return entries_.erase(id) != 0; }
void ResourceManager::clear() { std::lock_guard lock(mutex_); entries_.clear(); }
ResourceStats ResourceManager::stats() const {
    std::lock_guard lock(mutex_); ResourceStats s; s.count=entries_.size();
    for (const auto& [id,e] : entries_) { (void)id; s.bytes += e.info.bytes; switch(e.info.kind) { case ResourceKind::Buffer:++s.buffers;break; case ResourceKind::Texture:++s.textures;break; case ResourceKind::Sampler:++s.samplers;break; case ResourceKind::Shader:++s.shaders;break; case ResourceKind::Pipeline:++s.pipelines;break; default:break; } }
    return s;
}
std::vector<ResourceInfo> ResourceManager::snapshot() const { std::lock_guard lock(mutex_); std::vector<ResourceInfo> out; out.reserve(entries_.size()); for(const auto& [id,e]:entries_){(void)id;out.push_back(e.info);} std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.id<b.id;});return out; }
}
