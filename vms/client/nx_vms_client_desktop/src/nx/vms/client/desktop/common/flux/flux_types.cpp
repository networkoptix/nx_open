// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "flux_types.h"

#include <nx/reflect/to_string.h>

namespace nx::vms::client::desktop {

void PrintTo(CombinedValue value, ::std::ostream* os)
{
    *os << nx::reflect::toString(value);
}

} // namespace nx::vms::client::desktop
