#include "anvil/Vegetation.h"
#include <cassert>
int main() {
    anvil::VegetationSystem a(1234), b(1234);
    auto x=a.generate(-10,-10,100,20,4);
    auto y=b.generate(-10,-10,100,20,4);
    assert(x.size()==100 && y.size()==100);
    for (std::size_t i=0;i<x.size();++i) {
        assert(x[i].x==y[i].x && x[i].z==y[i].z && x[i].type==y[i].type);
        assert(x[i].x>=-10 && x[i].x<=10 && x[i].z>=-10 && x[i].z<=10);
    }
    auto visible=a.cull(x,{0,0,0,5,1.0f});
    assert(visible.size()<x.size());
    auto none=a.cull(x,{0,0,0,1000,0.0f});
    assert(none.empty());
}
