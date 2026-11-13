#include "anvil/Navigation.h"
#include <vector>
#include <cassert>
#include <limits>
#include <stdexcept>
int main(){anvil::NavigationGrid g(5,5);g.block(2,1);g.block(2,2);g.block(2,3);std::vector<anvil::GridPoint>p;assert(g.findPath({0,0},{4,4},p));assert(p.front().x==0&&p.back().x==4&&p.size()>=9);anvil::NavigationGrid invalid(-1,5);assert(!invalid.valid());assert(!invalid.findPath({0,0},{0,0},p));bool threw=false;try{anvil::NavigationGrid huge(std::numeric_limits<int>::max(),std::numeric_limits<int>::max());}catch(const std::length_error&){threw=true;}assert(threw);}
