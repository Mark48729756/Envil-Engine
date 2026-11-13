#include "anvil/Cinematic.h"
#include <cassert>
#include <cmath>
#include <limits>
int main(){anvil::CinematicSettings s;assert(s.valid());assert(anvil::circleOfConfusion(10,s)>0);assert(anvil::circleOfConfusion(std::numeric_limits<float>::quiet_NaN(),s)==0);s.focusDistance=s.focalLengthMm/1000.0f;assert(std::isfinite(anvil::circleOfConfusion(10,s)));s.aperture=0;assert(!s.valid());}
