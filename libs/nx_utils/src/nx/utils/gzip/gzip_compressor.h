// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include "../buffer.h"

namespace nx {
namespace utils {
namespace bstream {
namespace gzip {

class NX_UTILS_API Compressor
{
public:
    static bool isZlibCompressed(const nx::Buffer& data);

    static QByteArray compressData(const QByteArray& data, bool addCrcAndSize = true);
    static nx::Buffer compressData(const nx::Buffer& data, bool addCrcAndSize = true);

    static QByteArray deflateData(const QByteArray& data);
    static nx::Buffer deflateData(const nx::Buffer& data);

    static nx::Buffer uncompressData(const nx::Buffer& data);
};

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
