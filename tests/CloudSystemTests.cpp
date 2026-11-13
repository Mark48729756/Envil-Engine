#include "anvil/CloudSystem.h"
#include <cassert>
#include <cmath>
int main(){
    anvil::CloudParameters p; p.baseHeight=100; p.thickness=500; p.coverage=1.0f;
    anvil::CloudSystem clouds(p);
    auto a=clouds.sample(10,350,20,0), b=clouds.sample(10,50,20,0);
    assert(a.heightFraction>0.0f && a.heightFraction<1.0f);
    assert(b.density==0.0f);
    assert(a.density>=0.0f && a.density<=1.0f);
    assert(clouds.shadowTransmittance(10,20,0,0.7f)>0.0f);
    p.coverage=-5; clouds.setParameters(p); assert(clouds.parameters().coverage==0.0f);
}
