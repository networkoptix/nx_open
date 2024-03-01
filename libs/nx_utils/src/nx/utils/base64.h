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
    char* outBuf, int outBufCapacity);

/**
 * Base64 with URL-safe alphabet.
 * @return If outBuf != NULL, number of bytes written to outBuf is returned.
 * If output buffer size is insufficient, then -1 is returned.
 * If outBuf == NULL, returns number of bytes required to store base64 representation of data.
 */
NX_UTILS_API int toBase64Url(
    const void* data, int size,
    char* outBuf, int outBufCapacity);

/**
 * @return If outBuf != NULL, number of bytes written to outBuf is returned.
 * If output buffer size is insufficient, then -1 is returned.
 * If outBuf == NULL, returns maximum number of bytes required to store decoded representation of data.
 * Note: this is a very rough estimate, slightly less bytes may be used when actually decoding.
 */
NX_UTILS_API int fromBase64(
    const char* data, int size,
    void* outBuf, int outBufCapacity);

/**
 * Base64 with URL-safe alphabet.
 * @return If outBuf != NULL, number of bytes written to outBuf is returned.
 * If output buffer size is insufficient, then -1 is returned.
 * If outBuf == NULL, returns number of bytes required to store decoded representation of data.
 */
NX_UTILS_API int fromBase64Url(
    const char* data, int size,
    void* outBuf, int outBufCapacity);

NX_UTILS_API int estimateBase64DecodedLen(const char* encoded, int size);

inline std::string toBase64(const std::string_view& str)
{
    std::string result;
    result.resize((std::size_t) toBase64(str.data(), (int) str.size(), nullptr, 0));
    result.resize((std::size_t) toBase64(str.data(), (int) str.size(), result.data(), (int) result.size()));
    return result;
}

inline std::string toBase64Url(const std::string_view& str)
{
    std::string result;
    result.resize((std::size_t) toBase64Url(str.data(), (int) str.size(), nullptr, 0));
    result.resize((std::size_t) toBase64Url(str.data(), (int) str.size(), result.data(), (int) result.size()));
    return result;
}

// Appends base64Url(str) to out.
inline void toBase64Url(const std::string_view& str, std::string* out)
{
    const auto len = (std::size_t) toBase64Url(str.data(), (int) str.size(), nullptr, 0);
    const auto pos = out->size();
    out->resize(out->size() + len);
    const auto actualLen = (std::size_t) toBase64Url(str.data(), (int) str.size(), out->data() + pos, len);

    if (actualLen < len)
        out->resize(out->size() - (len - actualLen));
}

inline std::string fromBase64(const std::string_view& str)
{
    std::string result;
    result.resize((std::size_t) fromBase64(str.data(), (int) str.size(), nullptr, 0));
    result.resize((std::size_t) fromBase64(str.data(), (int) str.size(), result.data(), (int) result.size()));
    return result;
}

inline std::string fromBase64Url(const std::string_view& str)
{
    std::string result;
    result.resize((std::size_t) fromBase64Url(str.data(), (int) str.size(), nullptr, 0));
    result.resize((std::size_t) fromBase64Url(str.data(), (int) str.size(), result.data(), (int) result.size()));
    return result;
}

inline std::string toBase64(const std::string& str)
{
    return toBase64((std::string_view) str);
}

inline std::string toBase64Url(const std::string& str)
{
    return toBase64Url((std::string_view) str);
}

inline std::string fromBase64(const std::string& str)
{
    return fromBase64((std::string_view) str);
}

inline std::string fromBase64Url(const std::string& str)
{
    return fromBase64Url((std::string_view) str);
}

} // namespace nx::utils
