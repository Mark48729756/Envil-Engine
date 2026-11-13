#include "anvil/TaskGraph.h"
#include <vector>
#include <cassert>
int main(){anvil::TaskGraph g;assert(g.add(1)&&g.add(2)&&g.add(3));assert(g.depends(2,1)&&g.depends(3,2));std::vector<unsigned>o;assert(g.order(o)&&o.size()==3&&o[0]==1);assert(!g.depends(1,3));}
