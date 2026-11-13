#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream out; out << in.rdbuf(); return out.str();
}
static bool containsAll(const std::string& source, const char* const* tokens, int count) {
    for (int i = 0; i < count; ++i) if (source.find(tokens[i]) == std::string::npos) return false;
    return true;
}
int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: rendering_shader_contract_tests GL_SHADER RHI_SHADER\n"; return 2; }
    const std::string gl = read(argv[1]), rhi = read(argv[2]);
    if (gl.empty() || rhi.empty()) { std::cerr << "could not read shader source\n"; return 2; }
    const char* required[] = {"solarDisk", "solarGlow", "specularAo", "ambientSpecular * specularAo"};
    if (!containsAll(gl, required, 4) || !containsAll(rhi, required, 4)) {
        std::cerr << "GL and RHI lighting shaders must keep the same sky and specular-AO features\n";
        return 1;
    }
    std::cout << "Rendering shader contract passed\n";
    return 0;
}
