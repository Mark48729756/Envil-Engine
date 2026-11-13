#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace anvil {
using AssetId = std::uint64_t;
enum class AssetType : std::uint8_t { Unknown, Texture, Model, Material, Scene, Shader };
struct AssetRecord { AssetId id=0; AssetType type=AssetType::Unknown; std::filesystem::path path; std::uintmax_t size=0; std::filesystem::file_time_type modified{}; };

// Lightweight, deterministic project asset index. Paths are always stored relative to the project root.
class AssetDatabase {
public:
    bool scan(const std::filesystem::path& projectRoot, std::string* error=nullptr);
    const AssetRecord* find(AssetId id) const;
    const AssetRecord* find(const std::filesystem::path& relativePath) const;
    const std::vector<AssetRecord>& assets() const { return assets_; }
    const std::filesystem::path& root() const { return root_; }
    static AssetType classify(const std::filesystem::path& path);
    static AssetId makeId(const std::filesystem::path& normalizedRelativePath);
private:
    std::filesystem::path root_;
    std::vector<AssetRecord> assets_;
    std::unordered_map<AssetId,std::size_t> byId_;
};
}
