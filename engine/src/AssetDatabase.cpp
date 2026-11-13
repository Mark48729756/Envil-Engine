#include "anvil/AssetDatabase.h"
#include <algorithm>
#include <cctype>
#include <system_error>

namespace anvil {
namespace {
std::string lower(std::string s) { std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return s; }
}
AssetType AssetDatabase::classify(const std::filesystem::path& p) {
    const auto ext=lower(p.extension().string());
    if(ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".ppm"||ext==".bmp") return AssetType::Texture;
    if(ext==".obj"||ext==".gltf"||ext==".glb") return AssetType::Model;
    if(ext==".mat") return AssetType::Material;
    if(ext==".scene"||ext==".anvil") return AssetType::Scene;
    if(ext==".vert"||ext==".frag"||ext==".glsl") return AssetType::Shader;
    return AssetType::Unknown;
}
AssetId AssetDatabase::makeId(const std::filesystem::path& p) {
    // FNV-1a 64-bit over normalized generic path; reserve zero as invalid.
    const std::string s=p.lexically_normal().generic_string(); std::uint64_t h=14695981039346656037ull;
    for(unsigned char c:s){h^=c;h*=1099511628211ull;} return h ? h : 1;
}
bool AssetDatabase::scan(const std::filesystem::path& projectRoot,std::string* error) {
    std::error_code ec; auto canonical=std::filesystem::weakly_canonical(projectRoot,ec);
    if(ec||!std::filesystem::is_directory(canonical,ec)){if(error)*error="Project root is not an accessible directory";return false;}
    std::vector<AssetRecord> next; std::unordered_map<AssetId,std::size_t> ids;
    for(std::filesystem::recursive_directory_iterator it(canonical,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)) {
        if(ec){ec.clear();continue;}
        if(it->is_symlink(ec)){ if(it->is_directory(ec)) it.disable_recursion_pending(); ec.clear(); continue; }
        if(ec){ec.clear();continue;}
        if(it->is_directory(ec)) { const auto name=it->path().filename().string(); if(name==".git"||name=="build"||name=="out"||name==".vs"||name=="CMakeFiles") it.disable_recursion_pending(); continue; }
        if(ec){ec.clear();continue;}
        if(!it->is_regular_file(ec)){ec.clear();continue;}
        auto rel=std::filesystem::relative(it->path(),canonical,ec); if(ec){ec.clear();continue;}
        AssetRecord r; r.path=rel.lexically_normal(); r.type=classify(r.path); r.size=it->file_size(ec); if(ec){ec.clear();r.size=0;} r.modified=it->last_write_time(ec); if(ec){ec.clear();}
        r.id=makeId(r.path); if(ids.contains(r.id)){if(error)*error="Asset ID hash collision detected";return false;}
        ids.emplace(r.id,next.size()); next.push_back(std::move(r));
    }
    std::sort(next.begin(),next.end(),[](const auto&a,const auto&b){return a.path.generic_string()<b.path.generic_string();});
    ids.clear(); for(std::size_t i=0;i<next.size();++i)ids.emplace(next[i].id,i);
    root_=canonical; assets_=std::move(next); byId_=std::move(ids); return true;
}
const AssetRecord* AssetDatabase::find(AssetId id) const {auto i=byId_.find(id);return i==byId_.end()?nullptr:&assets_[i->second];}
const AssetRecord* AssetDatabase::find(const std::filesystem::path& p) const {auto n=p.lexically_normal();for(const auto&a:assets_)if(a.path==n)return &a;return nullptr;}
}
