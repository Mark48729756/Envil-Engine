#include "anvil/Ragdoll.h"
#include <cassert>
#include <cmath>
int main(){anvil::Ragdoll r; auto a=r.addBody({{0,0,0},1,true}),b=r.addBody({{2,0,0},1,true}); assert(r.addJoint(a,b,1.0f,1.0f)); r.setActive(true); r.solve(8); const auto&v=r.bodies(); const float d=std::abs(v[1].position.x-v[0].position.x); assert(d<1.01f); r.setBlend(0.4f); assert(std::abs(r.blend()-0.4f)<1e-5f); r.setActive(false); const auto before=r.bodies()[1].position.x; r.solve(); assert(r.bodies()[1].position.x==before);}
