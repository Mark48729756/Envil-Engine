#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream f(argv[1]); if (!f) return 3;
    const std::string s((std::istreambuf_iterator<char>(f)), {});
    const char* required[] = {"#version 450", "DrawIndexedIndirectCommand", "VisibilityBuffer", "SourceCommands", "OutputCommands", "gl_GlobalInvocationID.x", "command.instanceCount = visible[id] != 0u ? command.instanceCount : 0u"};
    for (const char* token : required) if (s.find(token) == std::string::npos) { std::cerr << "Missing: " << token << '\n'; return 1; }
    std::cout << "Indirect draw shader contract OK\\n";
}
