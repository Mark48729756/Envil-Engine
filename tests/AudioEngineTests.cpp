#include "anvil/AudioEngine.h"
#include <cassert>
#include <limits>
int main(){anvil::AudioEngine a;assert(a.addEmitter({1,{0,0,-5},{0,0,0},1,1,50,0,false,true}));auto r=a.evaluate(1);assert(r.gain>0&&r.doppler>0);assert(!a.addEmitter({2,{0,0,0},{0,0,0},1,1,std::numeric_limits<float>::quiet_NaN(),0,false,true}));assert(a.evaluate(1,std::numeric_limits<float>::quiet_NaN()).gain==0);}
