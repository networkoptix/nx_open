// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

class IStringList: public Interface<IStringList>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IStringList"); }

    virtual int count() const = 0;

    /** @return Null if the index is invalid. */
    virtual const char* at(int index) const = 0;
};
using IStringList0 = IStringList;

} // namespace nx::sdk
