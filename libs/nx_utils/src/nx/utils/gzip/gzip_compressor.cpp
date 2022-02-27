// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gzip_compressor.h"

#include <nx/utils/crc32.h>
#include <nx/utils/byte_stream/custom_output_stream.h>
#include "gzip_uncompressor.h"

namespace nx {
namespace utils {
namespace bstream {
namespace gzip {

nx::Buffer Compressor::uncompressData(const nx::Buffer& data)
{
    return runBytesThroughFilter<Uncompressor>(data);
}

QByteArray Compressor::compressData(const QByteArray& data, bool addCrcAndSize)
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
    if (addCrcAndSize)
    {
        quint32 tmp = crc32(data.data(), static_cast<std::size_t>(data.size()));
        result.append((const char*) &tmp, sizeof(quint32));
        tmp = (quint32)data.size();
        result.append((const char*) &tmp, sizeof(quint32));
    }
    return result;
}

nx::Buffer Compressor::compressData(const nx::Buffer& data, bool addCrcAndSize)
{
    return nx::Buffer(compressData(
        QByteArray::fromRawData(data.data(), (int) data.size()), addCrcAndSize));
}

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
