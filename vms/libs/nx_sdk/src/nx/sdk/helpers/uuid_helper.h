#pragma once

#include <cstring>
#include <string>

#include <nx/sdk/uuid.h>

namespace nx {
namespace sdk {

namespace UuidHelper
{
    /** @param data Array of 16 bytes. */
    template<typename Byte>
    Uuid fromRawData(const Byte* data)
    {
        static_assert(sizeof(Byte) == 1, "Expected pointer to array of byte-sized items");
        Uuid result;
        memcpy(&result, data, sizeof(result));
        return result;
    }

    /** @return Null uuid on error. */
    Uuid fromStdString(const std::string& str);

    enum FormatOptions
    {
        none = 0,
        uppercase = 1 << 0,
        hyphens = 1 << 1,
        braces = 1 << 2,
        all = 0xFF
    };

    /** @return String representation according to RFC-1422. */
    std::string toStdString(const Uuid& uuid, FormatOptions formatOptions = FormatOptions::all);
}

} // namespace sdk
} // namespace nx

//-------------------------------------------------------------------------------------------------
// Functions that need to be in namespace std for compatibility with STL features.

namespace std {

inline std::ostream& operator<<(std::ostream& os, const nx::sdk::Uuid& uuid)
{
    return os << nx::sdk::UuidHelper::toStdString(uuid);
}

template<>
struct hash<nx::sdk::Uuid>
{
    size_t operator()(const nx::sdk::Uuid& uuid) const
    {
        size_t h = 0;
        for (const auto b: uuid)
            h = (h + (324723947 + b)) ^ 93485734985;
        return h;
    }
};

} // namespace std
