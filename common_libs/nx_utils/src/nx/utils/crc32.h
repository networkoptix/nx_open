#pragma once

#include <cstdint>
#include <string>

#if defined(crc32)
    #undef crc32
#endif

namespace nx {
namespace utils {

NX_UTILS_API std::uint32_t crc32(const char* data, std::size_t size);
NX_UTILS_API std::uint32_t crc32(const std::string& str);

} // namespace utils
} // namespace nx
