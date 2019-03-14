#pragma once

namespace nx {
namespace sdk {

class IStringList
{
public:
    virtual ~IStringList() = default;
    virtual int count() const = 0;
    virtual const char* at(int index) const = 0;
};

} // namespace sdk
} // namespace nx
