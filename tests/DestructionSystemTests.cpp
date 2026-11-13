#include "anvil/DestructionSystem.h"
#include <cassert>
#include <limits>
int main(){anvil::DestructionSystem s;assert(s.fracture({0,0,0},2,16,42));assert(s.chunks().size()==16);s.applyImpulse({0,0,0},10,5);s.update(1.0f/60);assert(s.deactivate(0));assert(!s.deactivate(0));assert(!s.fracture({0,0,0},std::numeric_limits<float>::quiet_NaN(),3));}
