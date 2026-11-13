#include "anvil/Terrain.h"
#include <cassert>
#include <cmath>
#include <vector>
int main() {
    anvil::HeightField h(3,3,{0,1,2, 0,1,2, 0,1,2},1.0f);
    assert(std::abs(h.sample(0.5f,0.5f)-0.5f)<0.001f);
    assert(std::abs(h.sample(-5,0)-0.0f)<0.001f);
    auto s=h.sampleWithNormal(1,1);
    assert(s.normalX<0.0f && s.normalY>0.0f);
    auto near=anvil::selectTerrainLod(10,{0,100,250},20);
    assert(near.level==0 && near.morph==0.0f);
    auto transition=anvil::selectTerrainLod(90,{0,100,250},20);
    assert(transition.level==0 && transition.morph>0.0f);
    auto far=anvil::selectTerrainLod(300,{0,100,250},20);
    assert(far.level==2);
}
