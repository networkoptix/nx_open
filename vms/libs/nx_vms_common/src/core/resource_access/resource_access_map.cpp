// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_map.h"

#include <nx/utils/range_adapters.h>

namespace nx::core::access {

ResourceAccessMap& operator+=(ResourceAccessMap& destination, const ResourceAccessMap& source)
{
    if (destination.empty())
    {
        destination = source;
    }
    else
    {
        for (const auto& [resource, permissions]: nx::utils::constKeyValueRange(source))
            destination[resource] |= permissions;
    }

    return destination;
}

} // namespace nx::core::access
