#include "anvil/BuildPipeline.h"
#include <cassert>
int main(){anvil::BuildManifest m{"Demo","Release","Desktop",{{"bin/game","game"}}};assert(m.valid());m.artifacts[0].destination="../escape";assert(!m.valid());m.artifacts[0].destination="..\\escape";assert(!m.valid());m.artifacts[0].destination="C:\\escape";assert(!m.valid());m.artifacts[0].destination="folder\\..\\escape";assert(!m.valid());}
