#pragma once

#include <array>
#include <cstring>

namespace nx {
namespace sdk {

/**
 * Universally Unique Identifier. Intended for arbitrary purposes.
 *
 * Is safe to pass between plugin and its host because C++ standard guarantees the binary layout
 * of std::array.
 *
 * Is binary-compatible (has the same binary layout) with the old SDK (struct NX_GUID).
 */
class Uuid: public std::array<uint8_t, 16>
{
public:
    using base_type = std::array<uint8_t, 16>;

    static constexpr int kSize = (int) std::tuple_size<base_type>();

    constexpr Uuid(
        uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
        uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7,
        uint8_t b8, uint8_t b9, uint8_t bA, uint8_t bB,
        uint8_t bC, uint8_t bD, uint8_t bE, uint8_t bF)
        :
        base_type({b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, bA, bB, bC, bD, bE, bF})
    {
    }

    explicit Uuid(const uint8_t (&byteArray)[kSize])
    {
        memcpy(data(), byteArray, kSize);
    }

    constexpr Uuid(): base_type{} {} //< All zeros.
    
    bool isNull() const { return *this == Uuid(); }
};

} // namespace sdk
} // namespace nx
