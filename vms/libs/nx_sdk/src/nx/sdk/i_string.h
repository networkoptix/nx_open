#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

class IString: public nx::sdk::Interface<IString>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IString"); }

    virtual const char* str() const = 0;
};

} // namespace sdk
} // namespace nx
