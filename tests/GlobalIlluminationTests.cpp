#include "anvil/GlobalIllumination.h"
#include <cassert>
#include <cmath>
int main(){
    anvil::SH9 sh{}; sh[0]={1.0f,0.5f,0.25f};
    auto c=anvil::evaluateSH9(sh,{0,1,0});
    assert(c.r>0 && c.g>0 && c.b>0);
    anvil::GIProbeSet probes; anvil::GIProbe p; p.position={0,0,0}; p.irradiance=sh; probes.add(p);
    auto sampled=probes.sample({0,0,0},{0,1,0});
    assert(std::abs(sampled.r-c.r)<1e-5f);
    assert(probes.sample({1000,0,0},{0,1,0}).r==0.0f);
    p.validity=0; probes.clear(); probes.add(p); assert(probes.sample({0,0,0},{0,1,0}).r==0.0f);
}
