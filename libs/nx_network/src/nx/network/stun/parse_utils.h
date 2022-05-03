// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstddef>

namespace nx::network::stun {

/**
 * STUN RFC requires all attribute values be aligned to 4-byte boundary.
 * So, they need to be padded with zeros.
 * @return valueByteCount aligned to the next 4-byte boundary.
 */
inline std::size_t addPadding(std::size_t valueByteCount)
{
    static constexpr std::size_t kAlignMask = 3;
    return (valueByteCount + kAlignMask) & ~kAlignMask;
}

} // namespace nx::network::stun
