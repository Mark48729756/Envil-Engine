#pragma once
#include "Scene.h"
#include <filesystem>
#include <string>

namespace anvil {
// Versioned, human-readable scene format. Loading is transactional: the live scene is
// replaced only after the complete document has been parsed and validated.
class SceneSerializer {
public:
    static constexpr unsigned kVersion = 5;   // v5: Rigidbody component
    static bool save(const Scene& scene, const std::filesystem::path& path, std::string* error = nullptr);
    static bool load(Scene& scene, const std::filesystem::path& path, std::string* error = nullptr);
    static std::string toJson(const Scene& scene);
    static bool fromJson(Scene& scene, const std::string& json, std::string* error = nullptr);
};
}
