#include "gzip_compressor.h"

#include <nx/utils/crc32.h>

namespace nx {
namespace utils {
namespace bstream {
namespace gzip {

QByteArray Compressor::compressData(const QByteArray& data)
{
    QByteArray result;

    static int QT_HEADER_SIZE = 4;
    static int ZLIB_HEADER_SIZE = 2;
    static int ZLIB_SUFFIX_SIZE = 4;
    static int GZIP_HEADER_SIZE = 10;
    const quint8 GZIP_HEADER[] = {
        0x1f, 0x8b      // gzip magic number
        , 8             // compress method "deflate"
        , 1             // text data
        , 0, 0, 0, 0    // timestamp is not set
        , 2             // maximum compression flag
        , 255           // unknown OS
    };

    QByteArray compressedData = qCompress(data);
    QByteArray cleanData = QByteArray::fromRawData(compressedData.data() + QT_HEADER_SIZE + ZLIB_HEADER_SIZE, 
        compressedData.size() - (QT_HEADER_SIZE + ZLIB_HEADER_SIZE + ZLIB_SUFFIX_SIZE));
    result.reserve(cleanData.size() + GZIP_HEADER_SIZE);
    result.append((const char*) GZIP_HEADER, GZIP_HEADER_SIZE);
    result.append(cleanData);
    quint32 tmp = crc32(data.data(), static_cast<std::size_t>(data.size()));
    result.append((const char*) &tmp, sizeof(quint32));
    tmp = (quint32)data.size();
    result.append((const char*) &tmp, sizeof(quint32));
    return result;
}

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
