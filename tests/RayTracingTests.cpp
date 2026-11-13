#include "anvil/RayTracing.h"
#include <cassert>
int main(){anvil::RayTracingSceneModel s;auto g=s.addGeometry({3,3});assert(g==0);assert(s.addInstance(g,1));assert(s.ready());assert(s.addGeometry({0,0})==UINT32_MAX);}
