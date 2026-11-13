#pragma once
#include <vector>
namespace anvil {
enum class GraphicsApi { Unknown, OpenGL, Vulkan, DirectX11, DirectX12 };
struct ApiSupport { GraphicsApi api=GraphicsApi::Unknown;bool available=false;int featureLevel=0; };
inline GraphicsApi chooseGraphicsApi(const std::vector<ApiSupport>&v){
    constexpr GraphicsApi order[]={GraphicsApi::DirectX12,GraphicsApi::Vulkan,GraphicsApi::DirectX11,GraphicsApi::OpenGL};
    for(auto api:order)for(const auto&s:v)if(s.api==api&&s.available)return api;
    return GraphicsApi::Unknown;
}
} // namespace anvil
