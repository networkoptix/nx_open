// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_servers_field.h"

#include "../aggregated_event.h"
#include "../utils/field.h"

namespace nx::vms::rules {

bool TargetServersField::useSource() const
{
    return m_useSource;
}

void TargetServersField::setUseSource(bool value)
{
    if (m_useSource != value)
    {
        m_useSource = value;
        emit useSourceChanged();
    }
}

QVariant TargetServersField::build(const AggregatedEventPtr& event) const
{
    auto serverIds = ids().values();

    if (useSource())
       serverIds << utils::getFieldValue(event, utils::kServerIdFieldName, nx::Uuid());

    // Removing duplicates and maintaining order.
    UuidSet uniqueIds;
    UuidList result;
    for (const auto id: serverIds)
    {
        if (!id.isNull() && !uniqueIds.contains(id))
        {
            result.push_back(id);
            uniqueIds.insert(id);
        }
    }

    return QVariant::fromValue(result);
}

QJsonObject TargetServersField::openApiDescriptor(const QVariantMap&)
{
    auto descriptor = utils::constructOpenApiDescriptor<TargetServersField>();
    descriptor[utils::kDescriptionProperty] =
        "Specifies the target servers for the action. "
        "If the <code>useSource</code> flag is set, "
        "the list of target server IDs (<code>ids</code>) can be omitted.";
    utils::updatePropertyForField(descriptor,
        "useSource",
        utils::kDescriptionProperty,
        "Controls whether the device IDs from the event itself "
        "should be merged with the list of target server IDs.");
    return descriptor;
}

} // namespace nx::vms::rules
