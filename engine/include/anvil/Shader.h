#pragma once
#include <glm/glm.hpp>
#include <string>

namespace anvil {

class Shader {
public:
    Shader(const std::string& vertFile, const std::string& fragFile);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void bind() const;
    void set(const char* name, int v) const;
    void set(const char* name, float v) const;
    void set(const char* name, const glm::vec2& v) const;
    void set(const char* name, const glm::vec3& v) const;
    void set(const char* name, const glm::mat4& v) const;
    void setArray(const char* name, const glm::vec3* v, int count) const;
    void setArray(const char* name, const glm::mat4* v, int count) const;
    void setArray(const char* name, const float* v, int count) const;

private:
    int loc(const char* name) const;
    unsigned id_ = 0;
};

} // namespace anvil
