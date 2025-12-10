// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

/**
 * Stable and deterministic hash function suitable for persistent storage.
 */
inline uint64_t hash(std::string_view key)
{
    // Use FNV-1 algorithm.
    constexpr uint64_t kOffset = 14695981039346656037ULL;
    constexpr uint64_t kPrime = 1099511628211ULL;
    uint64_t result = kOffset;
    for (uint64_t c: key)
    {
        result *= kPrime;
        result ^= c;
    }
    return result;
}
