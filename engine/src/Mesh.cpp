#include "anvil/Mesh.h"
#include <atomic>
#include <cmath>
#include <limits>
#include <algorithm>
#include <glm/gtx/norm.hpp>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <glad/gl.h>

namespace anvil {

Mesh::Mesh(const std::vector<Vertex>& v, const std::vector<uint32_t>& idx) : verts_(v), idx_(idx), count_((int)idx.size()) {
    static std::atomic<std::uint64_t> next{1};
    uid_ = next.fetch_add(1);
    if (!verts_.empty()) {
        glm::vec3 lo(std::numeric_limits<float>::max());
        glm::vec3 hi(std::numeric_limits<float>::lowest());
        for (const Vertex& vertex : verts_) {
            lo = glm::min(lo, vertex.pos);
            hi = glm::max(hi, vertex.pos);
        }
        boundsCenter_ = (lo + hi) * 0.5f;
        for (const Vertex& vertex : verts_)
            boundsRadius_ = std::max(boundsRadius_, glm::length(vertex.pos - boundsCenter_));
    }
}

void Mesh::createGl() const {
    glCreateVertexArrays(1, &vao_);
    glCreateBuffers(1, &vbo_);
    glCreateBuffers(1, &ebo_);
    glNamedBufferStorage(vbo_, verts_.size() * sizeof(Vertex), verts_.data(), 0);
    glNamedBufferStorage(ebo_, idx_.size() * sizeof(uint32_t), idx_.data(), 0);
    glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(vao_, ebo_);
    glEnableVertexArrayAttrib(vao_, 0); glVertexArrayAttribFormat(vao_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));    glVertexArrayAttribBinding(vao_, 0, 0);
    glEnableVertexArrayAttrib(vao_, 1); glVertexArrayAttribFormat(vao_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal)); glVertexArrayAttribBinding(vao_, 1, 0);
    glEnableVertexArrayAttrib(vao_, 2); glVertexArrayAttribFormat(vao_, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));     glVertexArrayAttribBinding(vao_, 2, 0);
}

Mesh::~Mesh() {
    if (!vao_) return;   // never drawn through OpenGL (e.g. Vulkan build) -> nothing to free, no GL context needed
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ebo_);
    glDeleteVertexArrays(1, &vao_);
}

void Mesh::draw() const {
    if (!vao_) createGl();
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
}

std::unique_ptr<Mesh> Mesh::sphere(int seg) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    const float pi = 3.14159265358979f;
    for (int y = 0; y <= seg; ++y) {
        for (int x = 0; x <= seg; ++x) {
            float u = (float)x / seg, w = (float)y / seg;
            glm::vec3 p(std::cos(u * 2 * pi) * std::sin(w * pi), std::cos(w * pi), std::sin(u * 2 * pi) * std::sin(w * pi));
            v.push_back({p, p, {u, w}});
        }
    }
    for (int y = 0; y < seg; ++y)
        for (int x = 0; x < seg; ++x) {
            uint32_t a = y * (seg + 1) + x, b = a + seg + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    return std::make_unique<Mesh>(v, idx);
}

std::unique_ptr<Mesh> Mesh::plane(float h, float uvs) {
    glm::vec3 n(0, 1, 0);
    std::vector<Vertex> v = {{{-h, 0, -h}, n, {0, 0}}, {{h, 0, -h}, n, {uvs, 0}}, {{h, 0, h}, n, {uvs, uvs}}, {{-h, 0, h}, n, {0, uvs}}};
    return std::make_unique<Mesh>(v, std::vector<uint32_t>{0, 2, 1, 0, 3, 2});
}

std::unique_ptr<Mesh> Mesh::cube() {
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {{{0,0,1},{1,0,0},{0,1,0}}, {{0,0,-1},{-1,0,0},{0,1,0}}, {{1,0,0},{0,0,-1},{0,1,0}},
                           {{-1,0,0},{0,0,1},{0,1,0}}, {{0,1,0},{1,0,0},{0,0,-1}}, {{0,-1,0},{1,0,0},{0,0,1}}};
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    for (const Face& f : faces) {
        uint32_t b = (uint32_t)v.size();
        v.push_back({f.n - f.u - f.v, f.n, {0, 0}});
        v.push_back({f.n + f.u - f.v, f.n, {1, 0}});
        v.push_back({f.n + f.u + f.v, f.n, {1, 1}});
        v.push_back({f.n - f.u + f.v, f.n, {0, 1}});
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    return std::make_unique<Mesh>(v, idx);
}


std::unique_ptr<Mesh> Mesh::loadObj(const std::filesystem::path& path, std::string* error) {
    auto fail=[&](const char* message)->std::unique_ptr<Mesh>{if(error)*error=message;return {};};
    std::ifstream file(path); if(!file)return fail("Cannot open OBJ file");
    std::vector<glm::vec3> positions,normals; std::vector<glm::vec2> uvs;
    std::vector<Vertex> vertices; std::vector<uint32_t> indices;
    struct Key { int p,t,n; bool operator==(const Key& o)const{return p==o.p&&t==o.t&&n==o.n;} };
    struct Hash { std::size_t operator()(const Key& k)const{return (static_cast<std::size_t>(k.p)*73856093u)^(static_cast<std::size_t>(k.t+1)*19349663u)^(static_cast<std::size_t>(k.n+1)*83492791u);} };
    std::unordered_map<Key,uint32_t,Hash> cache; std::string line;
    auto resolve=[](int i,std::size_t n)->int{return i>0?i-1:(i<0?static_cast<int>(n)+i:-1);};
    while(std::getline(file,line)){
        std::istringstream in(line);std::string tag;in>>tag;if(tag.empty()||tag[0]=='#')continue;
        if(tag=="v"){glm::vec3 v;if(!(in>>v.x>>v.y>>v.z))return fail("Malformed OBJ position");positions.push_back(v);}
        else if(tag=="vt"){glm::vec2 v;if(!(in>>v.x>>v.y))return fail("Malformed OBJ UV");uvs.push_back(v);}
        else if(tag=="vn"){glm::vec3 v;if(!(in>>v.x>>v.y>>v.z))return fail("Malformed OBJ normal");normals.push_back(glm::normalize(v));}
        else if(tag=="f"){
            std::vector<uint32_t> face;std::string item;
            while(in>>item){Key k{-1,-1,-1};std::size_t a=item.find('/'),b=a==std::string::npos?a:item.find('/',a+1);try{k.p=resolve(std::stoi(item.substr(0,a)),positions.size());if(a!=std::string::npos&&b>a+1)k.t=resolve(std::stoi(item.substr(a+1,b-a-1)),uvs.size());if(b!=std::string::npos&&b+1<item.size())k.n=resolve(std::stoi(item.substr(b+1)),normals.size());}catch(...){return fail("Malformed OBJ face index");}
                if(k.p<0||static_cast<std::size_t>(k.p)>=positions.size()||(k.t>=0&&static_cast<std::size_t>(k.t)>=uvs.size())||(k.n>=0&&static_cast<std::size_t>(k.n)>=normals.size()))return fail("OBJ face index out of range");
                auto it=cache.find(k);if(it==cache.end()){Vertex v{};v.pos=positions[k.p];v.uv=k.t>=0?uvs[k.t]:glm::vec2(0);v.normal=k.n>=0?normals[k.n]:glm::vec3(0);uint32_t id=static_cast<uint32_t>(vertices.size());vertices.push_back(v);cache.emplace(k,id);face.push_back(id);}else face.push_back(it->second);
            }
            if(face.size()<3)return fail("OBJ face has fewer than three vertices");for(std::size_t i=1;i+1<face.size();++i)indices.insert(indices.end(),{face[0],face[i],face[i+1]});
        }
    }
    if(positions.empty()||indices.empty())return fail("OBJ contains no renderable faces");
    // Fill missing normals with area-weighted triangle normals.
    for(std::size_t i=0;i<indices.size();i+=3){auto&a=vertices[indices[i]],&b=vertices[indices[i+1]],&c=vertices[indices[i+2]];glm::vec3 n=glm::cross(b.pos-a.pos,c.pos-a.pos);if(glm::length2(n)>1e-12f){if(glm::length2(a.normal)<1e-12f)a.normal+=n;if(glm::length2(b.normal)<1e-12f)b.normal+=n;if(glm::length2(c.normal)<1e-12f)c.normal+=n;}}
    for(auto&v:vertices)if(glm::length2(v.normal)<1e-12f)v.normal={0,1,0};else v.normal=glm::normalize(v.normal);
    return std::make_unique<Mesh>(vertices,indices);
}

} // namespace anvil
