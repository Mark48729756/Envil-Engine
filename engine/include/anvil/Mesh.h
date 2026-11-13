#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <filesystem>
#include <string>
#include <vector>

namespace anvil {

struct Vertex { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };

class Mesh {
public:
    Mesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& indices);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;   // OpenGL path: creates the GL buffers lazily on the first call (needs a current GL context)

    // CPU copy of the geometry (kept so RHI backends - Vulkan / D3D12 - can upload it without any OpenGL context).
    const std::vector<Vertex>& vertices() const { return verts_; }
    const std::vector<uint32_t>& indices() const { return idx_; }
    std::uint64_t uid() const { return uid_; }   // unique for the lifetime of the process (never reused, unlike addresses)
    const glm::vec3& boundsCenter() const { return boundsCenter_; }
    float boundsRadius() const { return boundsRadius_; }

    static std::unique_ptr<Mesh> sphere(int segments = 64);
    static std::unique_ptr<Mesh> plane(float halfSize = 10.0f, float uvScale = 10.0f);
    static std::unique_ptr<Mesh> cube();
    static std::unique_ptr<Mesh> loadObj(const std::filesystem::path& path, std::string* error = nullptr);

private:
    void createGl() const;
    std::vector<Vertex> verts_;
    std::vector<uint32_t> idx_;
    std::uint64_t uid_ = 0;
    glm::vec3 boundsCenter_{0.0f};
    float boundsRadius_ = 0.0f;
    mutable unsigned vao_ = 0, vbo_ = 0, ebo_ = 0;
    int count_ = 0;
};

} // namespace anvil
