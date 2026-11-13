#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
static std::string read(const char* p) { std::ifstream f(p); std::ostringstream s; s << f.rdbuf(); return f ? s.str() : std::string{}; }
static bool has(const std::string& s, const char* t) { return s.find(t) != std::string::npos; }
int main(int argc, char** argv) {
    if (argc != 4) { std::cerr << "usage: shadow_cascade_contract_tests LIGHTING_SHADER RENDERER_SOURCE SHADOW_MATH_HEADER\n"; return 2; }
    const auto shader=read(argv[1]), renderer=read(argv[2]), math=read(argv[3]);
    if (shader.empty() || renderer.empty() || math.empty()) return 2;
    const char* shaderTokens[]={"sampler2DArrayShadow uShadow","sampler2DArray uShadowRaw","uCascadeSplits","uCameraView","uShadowTexelWorld","blockerSum","filterRadius","texture(uShadow"};
    const char* rendererTokens[]={"GL_TEXTURE_2D_ARRAY","glTextureStorage3D","glNamedFramebufferTextureLayer","setArray(\"uLightVP[0]" ,"GL_TEXTURE_COMPARE_MODE, GL_NONE","glTextureView"};
    const char* mathTokens[]={"ShadowCascadeSet","buildShadowCascades","splitLambda","texelWorld","glm::ortho"};
    for (auto t:shaderTokens) if(!has(shader,t)){std::cerr<<"missing shader token: "<<t<<"\n";return 1;}
    for (auto t:rendererTokens) if(!has(renderer,t)){std::cerr<<"missing renderer token: "<<t<<"\n";return 1;}
    for (auto t:mathTokens) if(!has(math,t)){std::cerr<<"missing cascade math token: "<<t<<"\n";return 1;}
    std::cout<<"Cascaded shadow-map and PCSS source contracts passed.\n"; return 0;
}
