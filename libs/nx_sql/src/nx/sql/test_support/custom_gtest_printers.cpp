// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_gtest_printers.h"

#include <ostream>

namespace nx::sql {

void PrintTo(const DBResult val, ::std::ostream* os)
{
    *os << val.toString();
}

} // namespace nx::sql
