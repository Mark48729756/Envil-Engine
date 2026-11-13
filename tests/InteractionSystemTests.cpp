#include "anvil/InteractionSystem.h"
#include <cassert>
#include <limits>
int main(){anvil::InteractionSystem s;assert(s.add({1,{0,0,0},2,"Open",true}));assert(s.nearest({1,0,0},5)==1);assert(s.interact(1));assert(s.lastInteracted()==1);assert(!s.add({2,{0,0,0},std::numeric_limits<float>::quiet_NaN(),"bad",true}));}
