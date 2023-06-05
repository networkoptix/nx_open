// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uuid_helper.h"

#include <limits>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#include <nx/kit/debug.h>
#include <chrono>
#include <random>
#include <stdint.h>
#include <mutex>

namespace nx::sdk {

namespace UuidHelper {

Uuid fromStdString(const std::string& str)
{
    static const int kMinUuidStrSize = 32;

    if ((int) str.size() < kMinUuidStrSize)
        return Uuid();

    Uuid uuid;
    int currentByteIndex = 0;
    std::string currentByteString;
    for (const char c: str)
    {
        switch (c)
        {
            case '{': case '}': case '-': case '\t': case '\n': case 'r': case ' ':
                continue;
        }

        if (currentByteIndex >= (int) sizeof(Uuid))
            return Uuid();

        currentByteString += c;
        if (currentByteString.size() == 2)
        {
            char* pEnd = nullptr;
            errno = 0; //< Required before strtol().
            const long v = std::strtol(currentByteString.c_str(), &pEnd, /*base*/ 16);
            const bool hasError =
                v > std::numeric_limits<uint8_t>::max()
                || v < std::numeric_limits<uint8_t>::min()
                || errno != 0
                || *pEnd != '\0';

            if (hasError)
                return Uuid();

            uuid[currentByteIndex] = (uint8_t) v;
            ++currentByteIndex;
            currentByteString.clear();
        }
    }

    if (currentByteIndex != sizeof(Uuid))
        return Uuid();

    return uuid;
}

std::string toStdString(const Uuid& uuid, FormatOptions formatOptions)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    if (formatOptions & FormatOptions::braces)
        ss << '{';
    if (formatOptions & FormatOptions::uppercase)
        ss << std::uppercase;
    for (int i = 0; i < 4; ++i)
        ss << std::setw(2) << (int) uuid[i];
    if (formatOptions & FormatOptions::hyphens)
        ss << '-';
    for (int i = 0; i < 2; ++i)
        ss << std::setw(2) << (int) uuid[4 + i];
    if (formatOptions & FormatOptions::hyphens)
        ss << "-";
    for (int i = 0; i < 2; ++i)
        ss << std::setw(2) << (int) uuid[6 + i];
    if (formatOptions & FormatOptions::hyphens)
        ss << "-";
    for (int i = 0; i < 2; ++i)
        ss << std::setw(2) << (int) uuid[8 + i];
    if (formatOptions & FormatOptions::hyphens)
        ss << "-";
    for (int i = 0; i < 6; ++i)
        ss << std::setw(2) << (int) uuid[10 + i];
    if (formatOptions & FormatOptions::braces)
        ss << '}';

    return ss.str();
}

class RandomGenerator64Bit
{
public:
    RandomGenerator64Bit():
        m_generator(getSeed()),
        m_distibution(0, std::numeric_limits<uint64_t>::max())
    {
    }

    uint64_t value64()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_distibution(m_generator);
    }

    std::array<uint64_t, 2> value128()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return { m_distibution(m_generator), m_distibution(m_generator) };
    }

private:
    uint64_t getSeed()
    {
        #if defined(__arm__) || defined(__aarch64__)
            // It is known that on certain ARM platforms std::random_device has issues.
            const auto time = std::chrono::high_resolution_clock::now().time_since_epoch();
            return time.count() ^ (uintptr_t) this;
        #else
            std::random_device r;
            return r();
        #endif
    }

private:
    std::mt19937_64 m_generator;
    std::uniform_int_distribution<uint64_t> m_distibution;
    std::mutex m_mutex;
};

Uuid randomUuid()
{
    static RandomGenerator64Bit generator;

    Uuid uuid;
    memcpy(uuid.data(), generator.value128().data(), sizeof(Uuid));

    uint8_t* data = uuid.data();
    data[6] = (data[6] & 0x0F) | 0x40; //< 8-bit of the form 4x (4 is UUID version).
    data[8] = (data[8] & 0x3F) | 0x80; //< 8-bit in range [0x80, 0xBF].

    return uuid;
}

} // namespace UuidHelper

} // namespace nx::sdk
