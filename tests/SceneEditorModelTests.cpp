#include "anvil/SceneEditorModel.h"
#include <cassert>
int main(){anvil::SceneEditorModel e;assert(e.add({1,"Player"}));assert(e.add({2,"Door"}));assert(!e.add({1,"Duplicate"}));assert(e.select(1));assert(e.select(2,true));assert(e.selection().size()==2);assert(e.translateSelection({1,2,3}));assert(e.find(1)->transform.translation.y==2);assert(e.setLocked(2,true));assert(!e.translateSelection({1,0,0}));assert(e.find(1)->transform.translation.x==1);assert(e.setVisible(1,false)&&!e.find(1)->visible);}
