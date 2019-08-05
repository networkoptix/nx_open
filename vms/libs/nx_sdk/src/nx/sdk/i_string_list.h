// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

class IStringList: public Interface<IStringList>
{
public:
    virtual int count() const = 0;
    virtual const char* at(int index) const = 0;
};

} // namespace sdk
} // namespace nx
