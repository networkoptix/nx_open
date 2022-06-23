// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base64.h"

namespace nx::utils {

// TODO: #akolesnikov Get rid of QByteArray in base64 functions. It will save 2 allocations.
// TODO: #akolesnikov Introduce inplace conversion. That will allow implementing fromBase64(String&&)
// without allocations at all.
// TODO: #akolesnikov Provide single implementation.

int toBase64(
    const void* data, int size,
    char* outBuf, int outBufCapacity)
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

static int estimateDecodedLen(const char* encoded, int size)
{
    // Removing padding if present.
    while (size > 0 && encoded[size-1] == '=')
        --size;

    // one byte is encoded to 2 characters, 2 bytes are encoded to 3 characters.
    static constexpr int kRemainderLenToDataLen[] = {0, 1, 1, 2};

    return (size / 4) * 3 // full 4-byte words.
        + kRemainderLenToDataLen[size % 4];
}

int fromBase64(
    const char* data, int size,
    void* outBuf, int outBufCapacity)
{
    const auto decodedLength = estimateDecodedLen(data, size);

    if (outBuf == nullptr)
        return decodedLength;

    if (outBufCapacity < decodedLength)
        return -1;

    const auto decoded = QByteArray::fromBase64(QByteArray::fromRawData(data, size));
    memcpy(outBuf, decoded.data(), decoded.size());
    return decoded.size();
}

} // namespace nx::utils
