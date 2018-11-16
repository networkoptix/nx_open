#include "crc32.h"

#include <nx/utils/zlib.h>

static inline uLong zlib_crc32(uLong crc, const Bytef *buf, uInt len)
{
    return crc32(crc, buf, len);
}

namespace nx {
namespace utils {

#if defined(crc32)
    #undef crc32 //< crc32 can be a macro.
#endif

std::uint32_t crc32(const char* data, std::size_t size)
{
    return (std::uint32_t) zlib_crc32(0, (const Bytef*) data, (uInt) size);
}

std::uint32_t crc32(const std::string& str)
{
    return crc32(str.data(), str.size());
}

uint32_t crc32(const QByteArray& data)
{
    return crc32(data.data(), (size_t) data.size());
}

uint32_t crc32(const QLatin1String& str)
{
    return crc32(str.data(), (size_t) str.size());
}

} // namespace utils
} // namespace nx
