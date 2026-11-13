#include "anvil/GpuProfiler.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace anvil {

GpuProfiler::GpuProfiler(float smoothing, float peakDecay)
    : smoothing_(std::clamp(smoothing, 0.001f, 1.0f)), peakDecay_(std::clamp(peakDecay, 0.0f, 1.0f)) {}

void GpuProfiler::reset() {
    scopes_.clear();
    serial_ = 0;
    frameMs_ = frameAvgMs_ = 0.0f;
}

bool GpuProfiler::update(const rhi::Device& device) {
    available_ = device.supportsGpuTimings();
    if (!available_) return false;
    return ingest(device.gpuTimings(), device.gpuTimingsFrame());
}

bool GpuProfiler::ingest(const std::vector<rhi::GpuScopeTiming>& timings, std::uint64_t serial) {
    available_ = true;
    if (serial == serial_ || timings.empty()) return false;
    serial_ = serial;

    // Same layout as last frame (same scopes, same order, same nesting) -> smooth in place. Otherwise (a pass was toggled)
    // restart the averages from the new frame so stale numbers never linger.
    bool sameLayout = timings.size() == scopes_.size();
    for (std::size_t i = 0; sameLayout && i < timings.size(); ++i)
        sameLayout = timings[i].depth == scopes_[i].depth && std::strncmp(timings[i].name, scopes_[i].name, rhi::kGpuScopeNameLength) == 0;

    if (!sameLayout) scopes_.assign(timings.size(), Scope{});
    float total = 0.0f;
    for (std::size_t i = 0; i < timings.size(); ++i) {
        Scope& s = scopes_[i];
        const rhi::GpuScopeTiming& t = timings[i];
        if (!sameLayout) {
            std::memcpy(s.name, t.name, sizeof s.name);
            s.depth = t.depth;
            s.avgMs = t.ms;
            s.maxMs = t.ms;
        } else {
            s.avgMs += (t.ms - s.avgMs) * smoothing_;
            s.maxMs = std::max(t.ms, s.maxMs * peakDecay_);
        }
        s.ms = t.ms;
        if (t.depth == 0) total += t.ms;
    }
    frameMs_ = total;
    frameAvgMs_ = sameLayout ? frameAvgMs_ + (total - frameAvgMs_) * smoothing_ : total;
    return true;
}

const GpuProfiler::Scope* GpuProfiler::find(const char* name) const noexcept {
    if (!name) return nullptr;
    for (const Scope& s : scopes_)
        if (std::strncmp(s.name, name, rhi::kGpuScopeNameLength) == 0) return &s;
    return nullptr;
}

std::size_t GpuProfiler::format(char* out, std::size_t capacity, float budgetMs) const {
    if (!out || capacity == 0) return 0;
    out[0] = '\0';
    std::size_t used = 0;
    auto put = [&](const char* fmt, auto... args) {
        if (used + 1 >= capacity) return;
        const int n = std::snprintf(out + used, capacity - used, fmt, args...);
        if (n > 0) used = std::min(capacity - 1, used + static_cast<std::size_t>(n));
    };
    if (!available_) { put("%s", "GPU timings unavailable on this backend\n"); return used; }
    put("GPU %.3f ms  (avg %.3f / budget %.1f)%s\n", frameMs_, frameAvgMs_, budgetMs, overBudget(budgetMs) ? "  OVER BUDGET" : "");
    for (const Scope& s : scopes_)
        put("%*s%-*s %7.3f ms  (avg %.3f, max %.3f)\n", static_cast<int>(s.depth * 2 + 2), "", static_cast<int>(18 - std::min<std::uint32_t>(s.depth * 2, 12)), s.name, s.ms, s.avgMs, s.maxMs);
    return used;
}

} // namespace anvil
