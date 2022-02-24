// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

namespace nx::reflect::test {

std::string toString(const C& c)
{
    return c.str;
}

bool fromString(const std::string& str, C* c)
{
    c->str = str;
    return true;
}

} // namespace nx::reflect::test
