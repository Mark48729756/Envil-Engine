#pragma once
// Open-world settings + cell math. World is split into square XZ cells; objects outside the
// stream radius around the camera are deactivated (no render, no physics) with hysteresis.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <glm/glm.hpp>
namespace anvil {
struct OpenWorldSettings {
    bool enabled = false;
    float cellSize = 128.0f;
    int loadRadius = 3, unloadRadius = 4;
    int farRadius = 10;                     // far-LOD proxy range in cells (0 = off)   // in cells (Chebyshev); unload > load = hysteresis
    void clampValues() { cellSize = std::clamp(cellSize, 16.0f, 4096.0f); loadRadius = std::clamp(loadRadius, 1, 32); unloadRadius = std::max(unloadRadius, loadRadius + 1); farRadius = std::clamp(farRadius, 0, 24); }
    bool save(const std::filesystem::path& p) const {
        std::error_code ec; std::filesystem::create_directories(p.parent_path(), ec);
        std::ofstream f(p); if (!f) return false;
        f << "enabled=" << (enabled ? 1 : 0) << "\ncell_size=" << cellSize << "\nload_radius=" << loadRadius << "\nunload_radius=" << unloadRadius << "\nfar_radius=" << farRadius << "\n"; return true;
    }
    bool load(const std::filesystem::path& p) {
        std::ifstream f(p); if (!f) return false; std::string l;
        while (std::getline(f, l)) { auto e = l.find('='); if (e == std::string::npos) continue; std::string k = l.substr(0, e), v = l.substr(e + 1);
            if (k == "enabled") enabled = std::atoi(v.c_str()) != 0; else if (k == "cell_size") cellSize = std::strtof(v.c_str(), nullptr);
            else if (k == "load_radius") loadRadius = std::atoi(v.c_str()); else if (k == "unload_radius") unloadRadius = std::atoi(v.c_str()); else if (k == "far_radius") farRadius = std::atoi(v.c_str()); }
        clampValues(); return true;
    }
};
inline void worldCellOf(const glm::vec3& p, float cs, int& cx, int& cz) { cx = int(std::floor(p.x / cs)); cz = int(std::floor(p.z / cs)); }
inline int worldCellDistance(int ax, int az, int bx, int bz) { return std::max(std::abs(ax - bx), std::abs(az - bz)); }
} // namespace anvil
