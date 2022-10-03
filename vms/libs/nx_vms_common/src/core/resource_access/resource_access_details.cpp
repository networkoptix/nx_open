// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_details.h"

#include <nx/utils/range_adapters.h>

namespace nx::core::access {

ResourceAccessDetails& operator+=(
    ResourceAccessDetails& destination, const ResourceAccessDetails& source)
{
    if (destination.empty())
    {
        destination = source;
    }
    else
    {
        for (const auto& [subjectId, resources]: nx::utils::constKeyValueRange(source))
            destination[subjectId] += resources;
    }

    return destination;
}

} // namespace nx::core::access
