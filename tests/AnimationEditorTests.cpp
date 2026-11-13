#include "anvil/AnimationEditor.h"
#include <cassert>
#include <limits>
int main(){anvil::FloatAnimationTrack t;assert(t.insert(1,10)&&t.insert(0,0));assert(t.insert(2,20));assert(t.sample(1.5f)==15);assert(t.keys().size()==3);assert(t.sample(std::numeric_limits<float>::quiet_NaN())==0);}
