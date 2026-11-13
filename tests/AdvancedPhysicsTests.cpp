#include "anvil/AdvancedPhysics.h"
#include <cassert>
#include <cmath>
int main(){anvil::ConstraintSolver s;auto a=s.addParticle({{0,0,0},{0,0,0},0});auto b=s.addParticle({{2,0,0},{2,0,0},1});assert(!s.addDistance({0,9,1,1}));assert(s.addDistance({a,b,1,1}));s.step(1.0f/60,{0,0,0},12);float dx=s.particles()[1].position.x-s.particles()[0].position.x;assert(std::abs(dx-1.0f)<.01f);}
