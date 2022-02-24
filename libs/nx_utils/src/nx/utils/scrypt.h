// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include "exception.h"

namespace nx::scrypt {

class NX_UTILS_API Exception: public nx::utils::ContextedException
{
    using nx::utils::ContextedException::ContextedException;
};

/**
 * SCrypt algorithm options, see https://tools.ietf.org/html/rfc7914. The defaults are taken from
 * openssl example https://www.openssl.org/docs/man1.1.1/man7/scrypt.html.
 */
struct NX_UTILS_API Options
{
    /** Block size. */
    uint32_t r = 8;

    /** CPU/Memory cost, must be larger than 1, a power of 2, and less than 2^(128 * r / 8). */
    uint32_t N = 1024;

    /** The parallelization parameter is a positive integer less than or equal to
     *  ((2^32-1) * 32) / (128 * r) */
    uint32_t p = 16;

    /** Output key size, it is a positive integer less than or equal to (2^32 - 1) * 32. */
    size_t keySize = 32;

    bool operator==(const Options& rhs) const;

    void validateOrThrow() const;
    bool isValid() const;
    QString toString() const;
};

NX_UTILS_API std::string encodeOrThrow(
    const std::string& password,
    const std::string& salt,
    const Options& options = {});

NX_UTILS_API std::optional<std::string> encode(
    const std::string& password,
    const std::string& salt,
    const Options& options = {});

} // nx::scrypt
