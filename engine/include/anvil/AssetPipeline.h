#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
namespace anvil {
enum class AssetKind{Unknown,Model,Texture,Audio,Animation,Material};
inline AssetKind classifyAsset(const std::string&path){auto p=path.find_last_of('.');if(p==std::string::npos)return AssetKind::Unknown;std::string e=path.substr(p+1);std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return char(std::tolower(c));});if(e=="fbx"||e=="gltf"||e=="glb"||e=="obj")return AssetKind::Model;if(e=="png"||e=="jpg"||e=="jpeg"||e=="tga"||e=="ktx2")return AssetKind::Texture;if(e=="wav"||e=="ogg"||e=="flac")return AssetKind::Audio;if(e=="anim")return AssetKind::Animation;if(e=="mat")return AssetKind::Material;return AssetKind::Unknown;}
struct ImportOptions{bool generateNormals=true,generateTangents=true,flipUV=false;float scale=1.0f;};struct ImportedAsset{std::string source;AssetKind kind=AssetKind::Unknown;ImportOptions options{};bool valid()const{return !source.empty()&&kind!=AssetKind::Unknown&&options.scale>0;}};
}
