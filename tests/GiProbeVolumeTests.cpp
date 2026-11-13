#include "anvil/GiProbeVolume.h"
#include <cassert>
#include <limits>
#include <stdexcept>
int main(){anvil::GiProbeVolume v(2,2,2);assert(v.set(1,1,1,{1,2,3}));auto p=v.get(1,1,1);assert(p.g==2&&v.count()==8);assert(!v.set(3,0,0,{}));bool threw=false;try{anvil::GiProbeVolume huge(std::numeric_limits<std::size_t>::max(),2,2);}catch(const std::length_error&){threw=true;}assert(threw);}
