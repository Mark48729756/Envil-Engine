#pragma once
#include "anvil/PathSafety.h"
#include <string>
#include <vector>
namespace anvil {
struct SdkPackage {
    std::string version;std::vector<std::string>files;bool hasEditor=true,hasRuntime=true,hasHeaders=true;
    bool valid()const{if(version.empty()||files.empty()||!hasEditor||!hasRuntime||!hasHeaders)return false;for(const auto&f:files)if(!isSafeRelativePath(f))return false;return true;}
};
} // namespace anvil
