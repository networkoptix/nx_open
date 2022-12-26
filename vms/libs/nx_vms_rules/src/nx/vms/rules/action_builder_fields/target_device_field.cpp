// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_device_field.h"

#include "../utils/field.h"

namespace nx::vms::rules {

QVariant TargetDeviceField::build(const AggregatedEventPtr& event) const
{
    auto deviceIds = ids().values();

    if (useSource())
        deviceIds << utils::getDeviceIds(event);

    // Removing duplicates and maintaining order.
    QnUuidSet uniqueIds;
    QnUuidList result;
    for (const auto id : deviceIds)
    {
        if (!uniqueIds.contains(id))
        {
            result.push_back(id);
            uniqueIds.insert(id);
        }
    }

    return QVariant::fromValue(result);
}

} // namespace nx::vms::rules
