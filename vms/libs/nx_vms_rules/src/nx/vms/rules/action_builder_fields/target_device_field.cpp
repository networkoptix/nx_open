// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_device_field.h"

#include "../utils/resource.h"

namespace nx::vms::rules {

bool TargetDeviceField::useSource() const
{
    return m_useSource;
}

void TargetDeviceField::setUseSource(bool value)
{
    if (m_useSource != value)
    {
        m_useSource = value;
        emit useSourceChanged();
    }
}

QVariant TargetDeviceField::build(const AggregatedEventPtr& event) const
{
    auto deviceIds = ids().values();

    if (useSource())
        deviceIds << utils::getDeviceIds(event);

    // Removing duplicates and maintaining order.
    UuidSet uniqueIds;
    UuidList result;
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

QJsonObject TargetDeviceField::openApiDescriptor()
{
    return utils::constructOpenApiDescriptor<TargetDeviceField>();
}

} // namespace nx::vms::rules
