#include "anvil/GameplayFramework.h"
#include <cassert>
#include <climits>
int main(){anvil::GameplayState g;assert(g.addItem("key",2));assert(g.removeItem("key"));assert(g.count("key")==1);g.setFlag("door_open",true);auto s=g.save();g.removeItem("key");g.load(s);assert(g.count("key")==1&&g.flag("door_open"));assert(g.addItem("max",INT_MAX));assert(!g.addItem("max",1));anvil::GameplaySnapshot bad;bad.inventory["negative"]=-9;g.load(bad);assert(g.count("negative")==0);}
