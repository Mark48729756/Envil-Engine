#include "anvil/GpuProfiler.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

using namespace anvil;
using rhi::GpuScopeTiming;

static GpuScopeTiming scope(const char* name, unsigned depth, float ms) {
    GpuScopeTiming t; std::strncpy(t.name, name, sizeof t.name - 1); t.depth = depth; t.ms = ms; return t;
}
static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    GpuProfiler p(0.5f, 0.5f);
    assert(!p.available() && p.scopes().empty());

    // frame 1: first sight of a layout seeds avg/max with the sample; frame total counts depth-0 scopes only
    std::vector<GpuScopeTiming> f = {scope("G-buffer", 0, 2.0f), scope("Lighting", 0, 4.0f), scope("Bloom down", 1, 1.0f)};
    assert(p.ingest(f, 1));
    assert(p.available() && p.scopes().size() == 3);
    assert(near(p.frameMs(), 6.0f) && near(p.frameAvgMs(), 6.0f));
    assert(near(p.find("Lighting")->avgMs, 4.0f) && p.find("Bloom down")->depth == 1);

    // same serial = same frame -> ignored
    assert(!p.ingest(f, 1));

    // frame 2: same layout -> EMA (weight 0.5) and decaying peak
    f[1].ms = 2.0f;
    assert(p.ingest(f, 2));
    assert(near(p.find("Lighting")->avgMs, 3.0f));
    assert(near(p.find("Lighting")->maxMs, 2.0f));   // max(2.0, 4.0 * 0.5)
    assert(near(p.frameMs(), 4.0f) && near(p.frameAvgMs(), 5.0f));

    // layout change (a pass was toggled off) -> averages restart, nothing stale survives
    std::vector<GpuScopeTiming> g = {scope("G-buffer", 0, 2.5f)};
    assert(p.ingest(g, 3));
    assert(p.scopes().size() == 1 && near(p.scopes()[0].avgMs, 2.5f) && !p.find("Lighting"));

    // budget flag follows the smoothed frame time, not a single spike
    GpuProfiler q(1.0f);
    q.ingest({scope("Frame", 0, 20.0f)}, 1);
    assert(q.overBudget(16.6f) && !q.overBudget(25.0f));

    // text report: fits the buffer, NUL-terminated, indents nested scopes, never overflows
    char buf[512];
    const std::size_t n = p.format(buf, sizeof buf);
    assert(n == std::strlen(buf) && std::strstr(buf, "G-buffer"));
    char tiny[16];
    const std::size_t m = q.format(tiny, sizeof tiny);
    assert(m < sizeof tiny && tiny[m] == '\0');
    GpuProfiler none;
    none.format(buf, sizeof buf);
    assert(std::strstr(buf, "unavailable"));

    p.reset();
    assert(p.scopes().empty() && p.frameMs() == 0.0f);
    std::cout << "GpuProfiler tests passed\n";
}
