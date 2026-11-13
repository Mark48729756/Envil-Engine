#include "anvil/TerrainEditor.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
int main(){anvil::TerrainEditor t(8,8);assert(t.valid());t.sculpt(3,3,2,4);assert(t.get(3,3)>0);float h=t.sample(3,3);t.flatten(3,3,2,1,1);assert(t.sample(3,3)==1);assert(h>1);assert(t.sample(std::numeric_limits<float>::quiet_NaN(),0)==0);const auto before=t.get(3,3);t.flatten(3,3,2,std::numeric_limits<float>::quiet_NaN());assert(t.get(3,3)==before);anvil::TerrainEditor bad(2,2,std::numeric_limits<float>::quiet_NaN());assert(bad.get(0,0)==0);bool threw=false;try{anvil::TerrainEditor huge(std::numeric_limits<std::size_t>::max(),2);}catch(const std::length_error&){threw=true;}assert(threw);}
