#include "anvil/WaterSurface.h"
#include <cassert>
#include <cmath>
int main() {
    anvil::WaterSurface water;
    const auto a=water.sample(0,0,0), b=water.sample(3,2,1.5f);
    assert(std::isfinite(a.height) && std::isfinite(b.normalY));
    assert(a.normalY>0.0f && b.normalY>0.0f);
    assert(anvil::WaterSurface::fresnelSchlick(0.0f)>anvil::WaterSurface::fresnelSchlick(1.0f));
    assert(anvil::WaterSurface::shoreFoam(0.0f)==1.0f);
    assert(anvil::WaterSurface::shoreFoam(10.0f)==0.0f);
}
