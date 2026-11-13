#include "anvil/ReleaseSdk.h"
#include <cassert>
int main(){anvil::SdkPackage p{"1.0.0",{"include/anvil/Core.h","lib/anvil.lib"},true,true,true};assert(p.valid());p.files.push_back("../secret");assert(!p.valid());p.files.pop_back();p.files.push_back("..\\secret");assert(!p.valid());p.files.pop_back();p.files.push_back("C:\\secret");assert(!p.valid());}
