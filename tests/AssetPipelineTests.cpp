#include "anvil/AssetPipeline.h"
#include <cassert>
int main(){assert(anvil::classifyAsset("hero.FBX")==anvil::AssetKind::Model);assert(anvil::classifyAsset("albedo.ktx2")==anvil::AssetKind::Texture);anvil::ImportedAsset a{"hero.glb",anvil::AssetKind::Model,{}};assert(a.valid());}
