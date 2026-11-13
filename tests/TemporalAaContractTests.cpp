#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
static std::string read(const char* p) { std::ifstream f(p, std::ios::binary); std::ostringstream s; s << f.rdbuf(); return f ? s.str() : std::string{}; }
static bool has(const std::string& s, const char* token) { return s.find(token) != std::string::npos; }
int main(int argc, char** argv) {
    if (argc != 4) { std::cerr << "usage: temporal_aa_contract_tests POST_SHADER RHI_RENDERER_SOURCE RENDERER_HEADER\n"; return 2; }
    const auto shader = read(argv[1]);
    const auto renderer = read(argv[2]);
    const auto header = read(argv[3]);
    if (shader.empty() || renderer.empty() || header.empty()) return 2;
    const char* shaderTokens[] = {"uHistoryColor", "uHistoryDepth", "temporalResolve", "depthTolerance", "uTAAHistoryWeight", "history = clamp"};
    for (auto t : shaderTokens) if (!has(shader, t)) { std::cerr << "missing TAA shader contract: " << t << "\n"; return 1; }
    const char* cacheTokens[] = {"gpuCacheBudgetBytes", "gpuCacheBytes", "gpuMeshCacheEntries", "gpuTextureCacheEntries", "use_count() == 1"};
    for (auto t : cacheTokens) if (!has(renderer, t) && !has(header, t)) {
        std::cerr << "missing bounded-cache contract: " << t << "\n"; return 1;
    }
    std::cout << "Temporal resolve and bounded GPU-cache source contracts passed.\n";
    return 0;
}
