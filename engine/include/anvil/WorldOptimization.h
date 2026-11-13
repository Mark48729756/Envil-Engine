#pragma once

// CPU-side building blocks for large-world rendering. These utilities are backend-agnostic.
#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace anvil {

struct LodLevel {
    std::uint32_t level = 0;
    float enterDistance = 0.0f;
};

// Distances must be strictly increasing. Hysteresis prevents rapid LOD flicker at boundaries.
class LodSelector {
public:
    explicit LodSelector(std::vector<LodLevel> levels = {{0, 0.0f}, {1, 40.0f}, {2, 100.0f}}, float hysteresis = 0.10f)
        : levels_(std::move(levels)), hysteresis_(std::isfinite(hysteresis) ? std::clamp(hysteresis, 0.0f, 0.49f) : 0.10f) {
        levels_.erase(std::remove_if(levels_.begin(), levels_.end(), [](const auto& l) {
            return !std::isfinite(l.enterDistance) || l.enterDistance < 0.0f;
        }), levels_.end());
        if (levels_.empty()) levels_.push_back({0, 0.0f});
        std::sort(levels_.begin(), levels_.end(), [](const auto& a, const auto& b) { return a.enterDistance < b.enterDistance; });
        levels_.erase(std::unique(levels_.begin(), levels_.end(), [](const auto& a, const auto& b) { return a.enterDistance == b.enterDistance; }), levels_.end());
    }

    std::uint32_t select(float distance, std::uint32_t previous = 0) const {
        if (levels_.empty()) return 0;
        if (!std::isfinite(distance)) distance = 0.0f;
        distance = std::max(0.0f, distance);
        std::size_t target = 0;
        while (target + 1 < levels_.size() && distance >= levels_[target + 1].enterDistance) ++target;
        auto current = std::find_if(levels_.begin(), levels_.end(), [previous](const LodLevel& l) { return l.level == previous; });
        if (current != levels_.end()) {
            const std::size_t old = static_cast<std::size_t>(current - levels_.begin());
            if (target > old && old + 1 < levels_.size()) {
                const float boundary = levels_[old + 1].enterDistance;
                if (distance < boundary * (1.0f + hysteresis_)) target = old;
            } else if (target < old && old > 0) {
                const float boundary = levels_[old].enterDistance;
                if (distance > boundary * (1.0f - hysteresis_)) target = old;
            }
        }
        return levels_[target].level;
    }

    const std::vector<LodLevel>& levels() const { return levels_; }
private:
    std::vector<LodLevel> levels_;
    float hysteresis_ = 0.10f;
};

struct StreamedAsset {
    std::filesystem::path path;
    std::vector<std::uint8_t> bytes;
    std::string error;
};



struct LodTransition {
    std::uint32_t primary = 0;
    std::uint32_t secondary = 0;
    float secondaryWeight = 0.0f;
};

// Produces a stable two-level transition band for dithering/cross-fade in a renderer.
inline LodTransition selectLodTransition(const LodSelector& selector, float distance,
                                         std::uint32_t previous, float fadeWidth = 8.0f) {
    const auto selected = selector.select(distance, previous);
    const auto& levels = selector.levels();
    auto it = std::find_if(levels.begin(), levels.end(), [selected](const LodLevel& l) { return l.level == selected; });
    if (it == levels.end() || std::next(it) == levels.end() || !std::isfinite(distance) || !std::isfinite(fadeWidth) || fadeWidth <= 0.0f)
        return {selected, selected, 0.0f};
    const float start = std::next(it)->enterDistance - fadeWidth;
    const float weight = std::clamp((distance - start) / fadeWidth, 0.0f, 1.0f);
    if (weight <= 0.0f) return {selected, selected, 0.0f};
    return {selected, std::next(it)->level, weight};
}

struct WorldCellCoord {
    int x = 0, y = 0, z = 0;
    bool operator==(const WorldCellCoord&) const = default;
};
struct WorldCellCoordHash {
    std::size_t operator()(const WorldCellCoord& c) const noexcept {
        const auto x = static_cast<std::uint32_t>(c.x), y = static_cast<std::uint32_t>(c.y), z = static_cast<std::uint32_t>(c.z);
        std::size_t h = static_cast<std::size_t>(x) * 0x9e3779b1u;
        h ^= static_cast<std::size_t>(y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

// CPU-side world partition planner. It reports cells to load/unload; actual scene and GPU
// resource creation remains the responsibility of the main/render thread.
class WorldPartition {
public:
    explicit WorldPartition(float cellSize = 128.0f, int loadRadius = 2, int unloadRadius = 3)
        : cellSize_(std::isfinite(cellSize) && cellSize > 0.0f ? cellSize : 128.0f),
          loadRadius_(std::clamp(loadRadius, 0, 32)),
          unloadRadius_(std::clamp(std::max(loadRadius_, unloadRadius), loadRadius_, 64)) {}

    std::vector<WorldCellCoord> update(float x, float y, float z) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return {};
        const auto toCell = [this](float value, int& out) {
            const double cell = std::floor(static_cast<double>(value) / static_cast<double>(cellSize_));
            const double lo = static_cast<double>(std::numeric_limits<int>::min() + unloadRadius_);
            const double hi = static_cast<double>(std::numeric_limits<int>::max() - unloadRadius_);
            if (cell < lo || cell > hi) return false;
            out = static_cast<int>(cell); return true;
        };
        WorldCellCoord center;
        if (!toCell(x, center.x) || !toCell(y, center.y) || !toCell(z, center.z)) return {};
        std::unordered_set<WorldCellCoord, WorldCellCoordHash> wanted;
        for (int dz = -loadRadius_; dz <= loadRadius_; ++dz)
            for (int dy = -loadRadius_; dy <= loadRadius_; ++dy)
                for (int dx = -loadRadius_; dx <= loadRadius_; ++dx)
                    wanted.insert({center.x + dx, center.y + dy, center.z + dz});
        std::vector<WorldCellCoord> load;
        for (const auto& cell : wanted) if (!resident_.count(cell)) load.push_back(cell);
        for (auto it = resident_.begin(); it != resident_.end();) {
            const auto dx = std::llabs(static_cast<long long>(it->x) - center.x);
            const auto dy = std::llabs(static_cast<long long>(it->y) - center.y);
            const auto dz = std::llabs(static_cast<long long>(it->z) - center.z);
            if (std::max({dx, dy, dz}) > unloadRadius_) it = resident_.erase(it);
            else ++it;
        }
        for (const auto& cell : load) resident_.insert(cell);
        return load;
    }
    bool isResident(WorldCellCoord cell) const { return resident_.count(cell) != 0; }
    std::size_t residentCount() const { return resident_.size(); }
    float cellSize() const { return cellSize_; }
private:
    float cellSize_; int loadRadius_; int unloadRadius_;
    std::unordered_set<WorldCellCoord, WorldCellCoordHash> resident_;
};

// One background IO worker. GPU upload remains on the render thread after pollCompleted().
class AsyncAssetStreamer {
public:
    explicit AsyncAssetStreamer(std::size_t maxAssetBytes = 512u * 1024u * 1024u)
        : maxAssetBytes_(maxAssetBytes), worker_([this] { run(); }) {}
    ~AsyncAssetStreamer() { stop(); }
    AsyncAssetStreamer(const AsyncAssetStreamer&) = delete;
    AsyncAssetStreamer& operator=(const AsyncAssetStreamer&) = delete;

    bool request(const std::filesystem::path& path, int priority = 0) {
        const auto key = path.lexically_normal().generic_string();
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || key.empty() || pending_.count(key) || active_.count(key) || completedKeys_.count(key)) return false;
        Job job{path, priority, sequence_++};
        jobs_.push(std::move(job));
        pending_[key] = true;
        cv_.notify_one();
        return true;
    }

    std::vector<StreamedAsset> pollCompleted() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<StreamedAsset> out;
        out.swap(completed_);
        for (const auto& asset : out) completedKeys_.erase(asset.path.lexically_normal().generic_string());
        return out;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

private:
    struct Job { std::filesystem::path path; int priority; std::uint64_t sequence; };
    struct LowerPriority { bool operator()(const Job& a, const Job& b) const {
        return a.priority == b.priority ? a.sequence > b.sequence : a.priority < b.priority;
    }};
    void run() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                if (stopping_ && jobs_.empty()) return;
                job = jobs_.top(); jobs_.pop();
                const auto k = job.path.lexically_normal().generic_string();
                pending_.erase(k); active_[k] = true;
            }
            StreamedAsset result; result.path = job.path;
            std::ifstream file(job.path, std::ios::binary | std::ios::ate);
            if (!file) result.error = "Unable to open asset";
            else {
                const auto end = file.tellg();
                if (end < 0) result.error = "Unable to determine asset size";
                else if (static_cast<std::uintmax_t>(end) > maxAssetBytes_ ||
                         static_cast<std::uintmax_t>(end) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
                         static_cast<std::uintmax_t>(end) > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
                    result.error = "Asset exceeds configured size limit";
                } else {
                    result.bytes.resize(static_cast<std::size_t>(end));
                    file.seekg(0);
                    if (!result.bytes.empty() && !file.read(reinterpret_cast<char*>(result.bytes.data()), static_cast<std::streamsize>(result.bytes.size()))) {
                        result.bytes.clear(); result.error = "Unable to read complete asset";
                    }
                }
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                active_.erase(job.path.lexically_normal().generic_string());
                completedKeys_.insert(job.path.lexically_normal().generic_string());
                completed_.push_back(std::move(result));
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Job, std::vector<Job>, LowerPriority> jobs_;
    std::unordered_map<std::string, bool> pending_, active_;
    std::unordered_set<std::string> completedKeys_;
    std::vector<StreamedAsset> completed_;
    std::uint64_t sequence_ = 0;
    std::size_t maxAssetBytes_;
    bool stopping_ = false;
    std::thread worker_;
};

} // namespace anvil
