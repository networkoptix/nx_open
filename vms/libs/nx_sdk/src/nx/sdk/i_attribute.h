// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

class IAttribute: public Interface<IAttribute>
{
public:
    enum class Type: int
    {
        undefined,
        number,
        boolean,
        string,
        // TODO: Consider adding other specific types like DateTime, Coordinates, Temperature.
    };

    static auto interfaceId() { return InterfaceId("nx::sdk::IAttribute"); }

    virtual Type type() const = 0;
    virtual const char* name() const = 0;
    virtual const char* value() const = 0;
    virtual float confidence() const = 0;
};

} // namespace sdk
} // namespace nx
