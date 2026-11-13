#pragma once
#include "rhi/RHI.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace anvil {

// Smooths the per-frame GPU scope timings an rhi::Device resolves from timestamp queries (see rhi::Device::gpuTimings) and
// reports them for the editor viewport overlay / console. UI-agnostic: the editor reads scopes() and draws whatever it likes.
// Everything is preallocated after the first frame with a given scope layout; update() does not allocate in steady state.
class GpuProfiler {
public:
    struct Scope {
        char name[rhi::kGpuScopeNameLength] = {};
        std::uint32_t depth = 0;
        float ms = 0.0f;      // latest frame
        float avgMs = 0.0f;   // exponential moving average
        float maxMs = 0.0f;   // slowly decaying peak
    };

    // smoothing: EMA weight of the newest sample (0..1). peakDecay: per-frame multiplier applied to the held peak (0..1).
    explicit GpuProfiler(float smoothing = 0.08f, float peakDecay = 0.985f);

    // Pulls the newest resolved frame from the device. Returns true if a new frame was ingested.
    bool update(const rhi::Device& device);
    // Same, from raw data (unit tests, replays). `serial` must change whenever `timings` holds a different frame.
    bool ingest(const std::vector<rhi::GpuScopeTiming>& timings, std::uint64_t serial);

    bool available() const noexcept { return available_; }                 // device supports timestamp queries
    const std::vector<Scope>& scopes() const noexcept { return scopes_; }  // submission order
    float frameMs() const noexcept { return frameMs_; }                    // sum of depth-0 scopes, latest frame
    float frameAvgMs() const noexcept { return frameAvgMs_; }
    bool overBudget(float budgetMs = 16.6f) const noexcept { return frameAvgMs_ > budgetMs; }
    const Scope* find(const char* name) const noexcept;                    // first scope with this name, or null

    // Plain-text table ("  Lighting      1.234 ms  (avg 1.200, max 1.900)"), nested scopes indented. Returns bytes written (excl. NUL).
    std::size_t format(char* out, std::size_t capacity, float budgetMs = 16.6f) const;

    void reset();

private:
    float smoothing_, peakDecay_;
    bool available_ = false;
    std::uint64_t serial_ = 0;
    float frameMs_ = 0.0f, frameAvgMs_ = 0.0f;
    std::vector<Scope> scopes_;
};

} // namespace anvil
