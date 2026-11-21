#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace ipc {
namespace posix_ {
namespace detail {

inline constexpr std::uint64_t fnv1a_64(const std::string_view data) noexcept {
    std::uint64_t hash = 0xcBF29CE484222325ULL;
    for (const char c : data) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

inline std::array<char, 16> to_hex_string(std::uint64_t value) noexcept {
    static constexpr char digits[] = "0123456789ABCDEF";

    std::array<char, 16> buffer;
    for (int i = 15; i >= 0; --i) {
        buffer[static_cast<std::size_t>(i)] = digits[value & 0xF];
        value >>= 4;
    }
    return buffer;
}

// Returns a POSIX shared-memory/semaphore name with a leading slash. When LIBIPC_SHM_NAME_MAX is
// defined at configuration time, names longer than this value are shortened by replacing part of
// the name with the hash of the full name.
inline std::string make_shm_name(const std::string_view name) {
    std::string result;

    if (name[0] == '/') {
        result = name;
    } else {
        result.reserve(1 + name.size());
        result += '/';
        result += name;
    }

#if defined(LIBIPC_SHM_NAME_MAX) && LIBIPC_SHM_NAME_MAX > 0
    static constexpr std::size_t max_len = LIBIPC_SHM_NAME_MAX;
    static_assert(max_len >= 18,
                  "LIBIPC_SHM_NAME_MAX must be at least 18 ('/' + '_' + 16 hex digits)");
    static constexpr std::size_t max_preserved_name_len = max_len - 17; // '_' + 16 hex digits

    if (result.size() > max_len) {
        std::string shortened;
        shortened.reserve(max_len);
        shortened.append(result, 0, max_preserved_name_len);
        shortened += '_';
        const auto hex = to_hex_string(fnv1a_64(result));
        shortened.append(hex.data(), hex.size());

        result = std::move(shortened);
    }
#endif

    return result;
}

} // namespace detail
} // namespace posix_
} // namespace ipc
