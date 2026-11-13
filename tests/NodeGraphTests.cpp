#include "anvil/NodeGraph.h"
#include <cassert>
int main(){anvil::NodeGraph g;assert(g.addNode(1)&&g.addNode(2)&&g.addNode(3));assert(g.connect(1,2)&&g.connect(2,3));assert(!g.connect(3,1));assert(g.links().size()==2);}
