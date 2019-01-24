#include "uuid_helper.h"

#include <limits>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {

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

Uuid randomUuid()
{
    Uuid uuid;
    int index = 0;
    const auto add16Bits =
        [&uuid, &index](int value) //< Converts value to Big Endian.
        {
            NX_KIT_ASSERT(index >= 0);
            NX_KIT_ASSERT(index <= 7);
            NX_KIT_ASSERT(value >= 0);
            // NOTE: rand() only guarantees to yield 15 bits.
            uuid[index * 2 + 0] = value >> 8;
            uuid[index * 2 + 1] = value & 0xFF;
            ++index;
        };

    add16Bits(std::rand());
    add16Bits(std::rand());
    add16Bits(std::rand());
    add16Bits((std::rand() & 0x0FFF) | 0x4000); //< 16-bit of the form 4xxx (4 is UUID version).
    add16Bits(std::rand() % 0x3FFF + 0x8000); //< 16-bit in range [0x8000, 0xBFFF].
    add16Bits(std::rand());
    add16Bits(std::rand());
    add16Bits(std::rand());

    NX_KIT_ASSERT(index == 8);
    return uuid;
}

} // namespace UuidHelper

} // namespace sdk
} // namespace nx
