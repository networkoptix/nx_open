#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

class IStringMap: public nx::sdk::Interface<IStringMap>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IStringMap"); }

    virtual int count() const = 0;

    /** @return Null if the index is invalid. */
    virtual const char* key(int i) const = 0;

    /** @return Null if the index is invalid. */
    virtual const char* value(int i) const = 0;

    /** @return Null if not found or the key is null. */
    virtual const char* value(const char* key) const = 0;
};

} // namespace sdk
} // namespace nx
