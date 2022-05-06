// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base64.h"

namespace nx::utils {

// TODO: #akolesnikov Get rid of QByteArray in base64 functions. It will save 2 allocations.
// TODO: #akolesnikov Introduce inplace conversion. That will allow implementing fromBase64(String&&)
// without allocations at all.
// TODO: #akolesnikov Provide single implementation.

namespace {

int toBase64Internal(
    const void* data, int size,
    char* outBuf, int outBufCapacity, bool isUrl)
{
    const auto encodedLength = ((size / 3) + 1) * 4;

    if (outBuf == nullptr)
        return encodedLength;

    if (outBufCapacity < encodedLength)
        return -1;

    const auto encoded =
        QByteArray::fromRawData((const char*) data, size)
            .toBase64(isUrl ? (QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)
                            : QByteArray::Base64Encoding);
    memcpy(outBuf, encoded.data(), encoded.size());
    return encoded.size();
}

} // anonymous namespace

int toBase64(
    const void* data, int size,
    char* outBuf, int outBufCapacity)
{
    return toBase64Internal(data, size, outBuf, outBufCapacity, false);
}

int toBase64Url(
    const void* data, int size,
    char* outBuf, int outBufCapacity)
{
    return toBase64Internal(data, size, outBuf, outBufCapacity, true);
}

namespace {

int estimateDecodedLen(const char* encoded, int size)
{
    // Removing padding if present.
    while (size > 0 && encoded[size-1] == '=')
        --size;

    // one byte is encoded to 2 characters, 2 bytes are encoded to 3 characters.
    static constexpr int kRemainderLenToDataLen[] = {0, 1, 1, 2};

    return (size / 4) * 3 // full 4-byte words.
        + kRemainderLenToDataLen[size % 4];
}

int fromBase64UrlInternal(
    const char* data, int size,
    void* outBuf, int outBufCapacity, bool isUrl)
{
    const auto decodedLength = estimateDecodedLen(data, size);

    if (outBuf == nullptr)
        return decodedLength;

    if (outBufCapacity < decodedLength)
        return -1;

    const auto decoded = QByteArray::fromBase64(
        QByteArray::fromRawData((const char*) data, size),
        isUrl ? QByteArray::Base64UrlEncoding : QByteArray::Base64Encoding);
    memcpy(outBuf, decoded.data(), decoded.size());
    return decoded.size();
}

} // anonymous namespace

int fromBase64(
    const char* data, int size,
    void* outBuf, int outBufCapacity)
{
    return fromBase64UrlInternal(data, size, outBuf, outBufCapacity, false);
}

int fromBase64Url(
    const char* data, int size,
    void* outBuf, int outBufCapacity)
{
    return fromBase64UrlInternal(data, size, outBuf, outBufCapacity, true);
}

} // namespace nx::utils
