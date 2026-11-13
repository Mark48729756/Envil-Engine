#include "anvil/Vehicle.h"
#include <cassert>
#include <limits>
int main(){anvil::VehicleModel v;v.step(1,0,.5f,.016f);assert(v.speed()>0&&v.steeringAngle()>0);v.step(0,1,0,.016f);assert(v.speed()>=0);v.step(0,0,0,std::numeric_limits<float>::quiet_NaN());assert(v.speed()>=0);anvil::VehicleModel bad({-1,1,1,1,1});assert(!bad.valid());}
