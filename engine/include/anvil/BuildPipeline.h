#pragma once
#include "anvil/PathSafety.h"
#include <string>
#include <vector>
namespace anvil {
struct BuildArtifact { std::string source,destination; };
struct BuildManifest {
    std::string projectName,configuration="Release",target="Desktop";std::vector<BuildArtifact>artifacts;
    bool valid()const{if(projectName.empty()||configuration.empty()||target.empty()||artifacts.empty())return false;for(const auto&a:artifacts)if(a.source.empty()||!isSafeRelativePath(a.destination))return false;return true;}
};
} // namespace anvil
