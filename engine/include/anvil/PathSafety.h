#pragma once
#include <string_view>

namespace anvil {
// Validates a package-relative path using both POSIX and Windows separators.
// Rejects absolute paths, drive/ADS syntax, and traversal segments.
inline bool isSafeRelativePath(std::string_view path) noexcept {
    if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
    if (path.find('\0') != std::string_view::npos || path.find(':') != std::string_view::npos) return false;
    std::size_t begin = 0;
    while (begin <= path.size()) {
        const auto end = path.find_first_of("/\\", begin);
        const auto length = (end == std::string_view::npos ? path.size() : end) - begin;
        const auto segment = path.substr(begin, length);
        if (segment == ".." || segment == ".") return false;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return true;
}
} // namespace anvil
