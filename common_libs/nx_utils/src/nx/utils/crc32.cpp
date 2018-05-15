#include "crc32.h"

#include <QtCore/QtGlobal>

#if defined(__APPLE__) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(__aarch64__)
    #include <zlib.h>
#else
    #include <QtZlib/zlib.h>
#endif

static inline uLong zlib_crc32(uLong crc, const Bytef *buf, uInt len)
{
    return crc32(crc, buf, len);
}

namespace nx {
namespace utils {

#if defined(crc32)
    #undef crc32
#endif

std::uint32_t crc32(const char* data, std::size_t size)
{
    return zlib_crc32(0, (const Bytef*) data, (unsigned long) size);
}

std::uint32_t crc32(const std::string& str)
{
    return crc32(str.data(), str.size());
}

} // namespace utils
} // namespace nx
