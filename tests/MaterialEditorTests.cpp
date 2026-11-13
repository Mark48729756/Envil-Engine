#include "anvil/MaterialEditor.h"
#include <cassert>
#include <limits>
int main(){anvil::MaterialEditorModel m;assert(m.set("roughness",2,0,1));assert(m.get("roughness")==1);assert(m.addLayer("snow",.7f));assert(!m.set("",1));assert(!m.set("bad",1,std::numeric_limits<float>::quiet_NaN(),1));}
