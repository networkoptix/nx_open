// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

class IString: public nx::sdk::Interface<IString>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IString"); }

    /** Never null. */
    virtual const char* str() const = 0;
};
using IString0 = IString;

} // namespace nx::sdk
