#include "anvil/MultiApi.h"
#include <vector>
#include <cassert>
int main(){std::vector<anvil::ApiSupport>s={{anvil::GraphicsApi::OpenGL,true,45},{anvil::GraphicsApi::Vulkan,true,13},{anvil::GraphicsApi::DirectX12,true,12}};assert(anvil::chooseGraphicsApi(s)==anvil::GraphicsApi::DirectX12);assert(anvil::chooseGraphicsApi({})==anvil::GraphicsApi::Unknown);}
