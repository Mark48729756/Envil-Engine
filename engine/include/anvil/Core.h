#pragma once
#include <cstdio>
#include <cstdlib>

namespace anvil {
using LogSink = void (*)(const char*);
inline LogSink& logSink() { static LogSink s = nullptr; return s; }
} // namespace anvil

#define ANVIL_LOG(...) do { char anvilBuf_[2300]; std::snprintf(anvilBuf_, sizeof anvilBuf_, __VA_ARGS__); \
    std::printf("[ANVIL] %s\n", anvilBuf_); std::fflush(stdout); \
    if (::anvil::logSink()) ::anvil::logSink()(anvilBuf_); } while (0)
#define ANVIL_ASSERT(c, ...) do { if (!(c)) { ANVIL_LOG(__VA_ARGS__); std::abort(); } } while (0)
