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
    QByteArray deflatedData = Compressor::deflateData(data);

    constexpr int kGzipHeaderSize = 10;
    constexpr quint8 kGzipHeader[] = {
        0x1f, 0x8b      // gzip magic number
        , 8             // compress method "deflate"
        , 1             // text data
        , 0, 0, 0, 0    // timestamp is not set
        , 2             // maximum compression flag
        , 255           // unknown OS
    };
    QByteArray result;

    result.reserve(deflatedData.size() + kGzipHeaderSize);
    result.append((const char*) kGzipHeader, kGzipHeaderSize);
    result.append(deflatedData);
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

QByteArray Compressor::deflateData(const QByteArray& data)
{
    constexpr int kQtHeaderSize = 4;
    constexpr int kZlibHeaderSize = 2;
    constexpr int kZlibSuffixSize = 4;

    QByteArray compressedData = qCompress(data);

    return QByteArray(compressedData.data() + kQtHeaderSize + kZlibHeaderSize,
        compressedData.size() - (kQtHeaderSize + kZlibHeaderSize + kZlibSuffixSize));
}

nx::Buffer Compressor::deflateData(const nx::Buffer& data)
{
    return nx::Buffer(deflateData(
        QByteArray::fromRawData(data.data(), (int) data.size())));
}

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
