// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <QtCore/QByteArray>

namespace nx::utils {

/**
 * @return If outBuf != NULL, number of bytes written to outBuf is returned.
 * If output buffer size is insufficient, then -1 is returned.
 * If outBuf == NULL, returns number of bytes required to store base64 representation of data.
 */
NX_UTILS_API int toBase64(
    const void* data, int size,
    void* outBuf, int outBufCapacity);

/**
 * @return If outBuf != NULL, number of bytes written to outBuf is returned.
 * If output buffer size is insufficient, then -1 is returned.
 * If outBuf == NULL, returns number of bytes required to store decoded representation of data.
 */
NX_UTILS_API int fromBase64(
    const void* data, int size,
    void* outBuf, int outBufCapacity);

inline std::string toBase64(const std::string_view& str)
{
    std::string result;
    result.resize((std::size_t) toBase64(str.data(), (int) str.size(), nullptr, 0));
    result.resize((std::size_t) toBase64(str.data(), (int) str.size(), result.data(), (int) result.size()));
    return result;
}

inline std::string fromBase64(const std::string_view& str)
{
    std::string result;
    result.resize((std::size_t) fromBase64(str.data(), (int) str.size(), nullptr, 0));
    result.resize((std::size_t) fromBase64(str.data(), (int) str.size(), result.data(), (int) result.size()));
    return result;
}

inline std::string toBase64(const std::string& str)
{
    return toBase64((std::string_view) str);
}

inline std::string fromBase64(const std::string& str)
{
    return fromBase64((std::string_view) str);
}

} // namespace nx::utils
