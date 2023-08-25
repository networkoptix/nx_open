// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

class IAttribute: public Interface<IAttribute>
{
public:
    enum class Type: int
    {
        undefined,
        number,
        boolean,
        string,
        // TODO: Consider adding other specific types like Coordinates, Temperature.
    };

    static auto interfaceId() { return makeId("nx::sdk::IAttribute"); }

    virtual Type type() const = 0;

    /**
     * See the specification for Attribute names in @ref md_src_nx_sdk_attributes.
     */
    virtual const char* name() const = 0;

    virtual const char* value() const = 0;

    virtual float confidence() const = 0;
};
using IAttribute0 = IAttribute;

} // namespace nx::sdk
