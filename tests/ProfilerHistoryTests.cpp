#include "anvil/ProfilerHistory.h"
#include <cassert>
int main(){anvil::ProfilerHistory h(3);h.push(1);h.push(2);h.push(3);h.push(4);assert(h.size()==3&&h.average()==3&&h.percentile(1)==4);}
