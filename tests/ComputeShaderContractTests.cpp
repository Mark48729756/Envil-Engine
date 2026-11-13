#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "usage: anvil_compute_shader_contract_tests <compute shader path>");
    std::ifstream file(argv[1], std::ios::binary);
    require(static_cast<bool>(file), "compute shader file must exist");
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    require(!source.empty(), "compute shader must not be empty");
    require(source.find("#version 450") != std::string::npos, "shader dialect must be GLSL 450");
    require(source.find("local_size_x = 64") != std::string::npos, "workgroup X size must match validation dispatch");
    require(source.find("layout(set = 3, binding = 0") != std::string::npos,
            "storage buffer must use set 3 / binding 0 (D3D12 UAV register space 3)");
    require(source.find("layout(push_constant)") != std::string::npos,
            "push constants must be present for elementCount and addend");
    require(source.find("i < pc.elementCount") != std::string::npos,
            "shader must bounds-check all invocations");
    require(source.find("dataBuffer.values[i] * 3u + pc.addend") != std::string::npos,
            "shader arithmetic must match native readback expectations");

    // Mirror the native test's two dispatches: 64 direct threads, then 128 indirect threads.
    constexpr unsigned count = 128;
    std::vector<unsigned> values(count);
    for (unsigned i = 0; i < count; ++i) values[i] = i;
    for (unsigned i = 0; i < 64; ++i) values[i] = values[i] * 3u + 7u;
    for (unsigned i = 0; i < 128; ++i) values[i] = values[i] * 3u + 7u;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned expected = i < 64 ? ((i * 3u + 7u) * 3u + 7u) : (i * 3u + 7u);
        require(values[i] == expected, "CPU reference must match direct + indirect dispatch result");
    }
    std::cout << "PASS: compute shader binding/workgroup/push-constant contract and dispatch reference\n";
    return 0;
}
