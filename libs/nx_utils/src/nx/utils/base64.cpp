// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base64.h"

namespace nx::utils {

// TODO: #akolesnikov Get rid of QByteArray in base64 functions. It will save 2 allocations.
// TODO: #akolesnikov Introduce inplace conversion. That will allow implementing fromBase64(String&&)
// without allocations at all.
// TODO: #akolesnikov Provide single implementation.

int toBase64(
    const void* data, int size,
    void* outBuf, int outBufCapacity)
{
    const auto encodedLength = ((size / 3) + 1) * 4;

    if (outBuf == nullptr)
        return encodedLength;

    if (outBufCapacity < encodedLength)
        return -1;

    const auto encoded = QByteArray::fromRawData((const char*) data, size).toBase64();
    memcpy(outBuf, encoded.data(), encoded.size());
    return encoded.size();
}

int fromBase64(
    const void* data, int size,
    void* outBuf, int outBufCapacity)
{
    const auto decodedLength = ((size / 4) + 1) * 3;

    if (outBuf == nullptr)
        return decodedLength;

    if (outBufCapacity < decodedLength)
        return -1;

    const auto decoded = QByteArray::fromBase64(QByteArray::fromRawData((const char*) data, size));
    memcpy(outBuf, decoded.data(), decoded.size());
    return decoded.size();
}

} // namespace nx::utils
