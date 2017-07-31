#pragma once

namespace nx {
namespace sdk {
namespace  v0 {

struct Guid
{
    static const int kGuidLength = 16;
    unsigned char data[kGuidLength];
};

} // namesapce v0
} // namespace sdk
} // namespace nx
