#include "anvil/CollisionWorld.h"
#include <cassert>
#include <limits>
int main(){anvil::CollisionWorld w;assert(w.add({1,{{0,0,0},{1,1,1}},false,true}));assert(w.add({2,{{.5f,.5f,.5f},{2,2,2}},false,true}));assert(w.detect().size()==1);assert(!w.add({3,{{0,0,0},{std::numeric_limits<float>::quiet_NaN(),1,1}},false,true}));}
