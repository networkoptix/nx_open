// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_connection_data.h"

#include <nx/reflect/compare.h>

namespace nx::vms::client::core {

bool WeightData::operator==(const WeightData& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::client::core
