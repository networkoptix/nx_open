// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_devices_field.h"

#include "../utils/resource.h"

namespace nx::vms::rules {

bool TargetDevicesField::useSource() const
{
    return m_useSource;
}

void TargetDevicesField::setUseSource(bool value)
{
    if (m_useSource != value)
    {
        m_useSource = value;
        emit useSourceChanged();
    }
}

QVariant TargetDevicesField::build(const AggregatedEventPtr& event) const
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

QJsonObject TargetDevicesField::openApiDescriptor(const QVariantMap&)
{
    auto descriptor = utils::constructOpenApiDescriptor<TargetDevicesField>();
    descriptor[utils::kDescriptionProperty] =
        "Specifies the target resources for the action. "
        "If the <code>useSource</code> flag is set, "
        "the list of target device IDs (<code>ids</code>) can be omitted.";

    utils::updatePropertyForField(descriptor,
        "useSource",
        utils::kDescriptionProperty,
        "Controls whether the device IDs from the event itself "
        "should be merged with the list of target device IDs.");
    return descriptor;
}

} // namespace nx::vms::rules
