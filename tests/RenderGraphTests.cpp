#include "anvil/RenderGraph.h"
#include <string>
#include <vector>
#include <cassert>
int main(){
    anvil::RenderGraph g;assert(g.addPass({"gbuffer",{}, {"depth","color"}}));assert(g.addPass({"lighting",{"depth","color"},{"lit"}}));std::vector<std::string>o;assert(g.compile(o));assert(o.size()==2&&o[0]=="gbuffer");
    anvil::RenderGraph hazards;assert(hazards.addPass({"read",{"external"},{}}));assert(hazards.addPass({"overwrite",{}, {"external"}}));assert(hazards.compile(o));assert(o.size()==2&&o[0]=="read"&&o[1]=="overwrite");
}
