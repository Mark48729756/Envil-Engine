#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace anvil::rhi {
using ResourceId = std::uint64_t;
enum class ResourceKind : std::uint8_t { Buffer, Texture, Sampler, Shader, Pipeline, Other };
struct ResourceInfo { ResourceId id = 0; ResourceKind kind = ResourceKind::Other; std::string name; std::size_t bytes = 0; };
struct ResourceStats { std::size_t count = 0, bytes = 0; std::size_t buffers = 0, textures = 0, samplers = 0, shaders = 0, pipelines = 0; };

// Keeps GPU resources alive and provides a lightweight runtime inventory.
// Release resources while their graphics context is still current.
class ResourceManager {
public:
    template<class T> ResourceId retain(std::shared_ptr<T> resource, ResourceKind kind, std::string name, std::size_t bytes = 0) {
        if (!resource) return 0;
        std::lock_guard lock(mutex_); const ResourceId id = nextId_++;
        entries_.emplace(id, Entry{std::move(resource), std::type_index(typeid(T)), {id, kind, std::move(name), bytes}});
        return id;
    }
    template<class T> std::shared_ptr<T> get(ResourceId id) const {
        std::lock_guard lock(mutex_); auto it = entries_.find(id); if (it == entries_.end()) return {};
        if (it->second.type != std::type_index(typeid(T))) return {};
        return std::static_pointer_cast<T>(it->second.object);
    }
    bool release(ResourceId id);
    void clear();
    ResourceStats stats() const;
    std::vector<ResourceInfo> snapshot() const;
private:
    struct Entry { std::shared_ptr<void> object; std::type_index type{typeid(void)}; ResourceInfo info; };
    mutable std::mutex mutex_; ResourceId nextId_ = 1; std::unordered_map<ResourceId, Entry> entries_;
};
}
