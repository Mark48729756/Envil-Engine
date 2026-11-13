#include "anvil/Shader.h"
#include "anvil/Core.h"
#include <fstream>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>

namespace anvil {

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    ANVIL_ASSERT(f.good(), "Cannot open shader: %s", path.c_str());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile(GLenum type, const std::string& file) {
    std::string src = readFile(file);
    const char* c = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        ANVIL_ASSERT(false, "Shader compile error in %s:\n%s", file.c_str(), log);
    }
    return s;
}

Shader::Shader(const std::string& vertFile, const std::string& fragFile) {
    GLuint v = compile(GL_VERTEX_SHADER, vertFile);
    GLuint f = compile(GL_FRAGMENT_SHADER, fragFile);
    id_ = glCreateProgram();
    glAttachShader(id_, v);
    glAttachShader(id_, f);
    glLinkProgram(id_);
    GLint ok = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
        ANVIL_ASSERT(false, "Shader link error:\n%s", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
}

Shader::~Shader() { glDeleteProgram(id_); }
void Shader::bind() const { glUseProgram(id_); }
int Shader::loc(const char* n) const { return glGetUniformLocation(id_, n); }
void Shader::set(const char* n, int v) const { glProgramUniform1i(id_, loc(n), v); }
void Shader::set(const char* n, float v) const { glProgramUniform1f(id_, loc(n), v); }
void Shader::set(const char* n, const glm::vec2& v) const { glProgramUniform2fv(id_, loc(n), 1, &v.x); }
void Shader::set(const char* n, const glm::vec3& v) const { glProgramUniform3fv(id_, loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::mat4& v) const { glProgramUniformMatrix4fv(id_, loc(n), 1, GL_FALSE, glm::value_ptr(v)); }
void Shader::setArray(const char* n, const glm::vec3* v, int c) const { glProgramUniform3fv(id_, loc(n), c, glm::value_ptr(*v)); }
void Shader::setArray(const char* n, const glm::mat4* v, int c) const { glProgramUniformMatrix4fv(id_, loc(n), c, GL_FALSE, glm::value_ptr(*v)); }
void Shader::setArray(const char* n, const float* v, int c) const { glProgramUniform1fv(id_, loc(n), c, v); }

} // namespace anvil
