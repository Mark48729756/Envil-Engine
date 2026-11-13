#include "anvil/MaterialLayer.h"
#include <cassert>
#include <cmath>
int main() {
    const std::vector<float> heights{0.0f,0.2f,0.4f,0.6f,0.8f,1.0f};
    const float offset=anvil::parallaxOcclusionOffset(0.5f,0.8f,0.04f,heights,24);
    assert(std::isfinite(offset) && offset<=0.0f);
    const auto blend=anvil::blendMaterialLayers({{0.2f,2.0f,1.0f,1},{0.8f,2.0f,2.0f,2}},0.5f);
    assert(blend.size()==2);
    assert(std::abs(blend[0].weight+blend[1].weight-1.0f)<0.001f);
    assert(anvil::triplanarWeight(1.0f)>anvil::triplanarWeight(0.1f));
    assert(anvil::blendMaterialLayers({},0.5f).empty());
}
