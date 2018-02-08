#include "crc32.h"

#if defined(Q_OS_MACX) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#include <zlib.h>
#else
#include <QtZlib/zlib.h>
#endif

// On some platforms zconf.h defined crc32 as z_crc32, sometimes - not.
#ifdef crc32
#   undef crc32
#else
#   define z_crc32 crc32
#endif

namespace nx {
namespace utils {

std::uint32_t crc32(const char* data, std::size_t size)
{
    return ::z_crc32(0, (const Bytef*)data, (unsigned long)size);
}

std::uint32_t crc32(const std::string& str)
{
    return crc32(str.data(), str.size());
}

} // namespace utils
} // namespace nx
