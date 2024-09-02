// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_single_device_field.h"

#include <core/resource/network_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/openapi_doc.h>

#include "../manifest.h"
#include "../utils/resource.h"

namespace nx::vms::rules {

nx::Uuid TargetSingleDeviceField::id() const
{
    return m_id;
}

void TargetSingleDeviceField::setId(nx::Uuid id)
{
    if (m_id == id)
        return;

    m_id = id;
    emit idChanged();
}

bool TargetSingleDeviceField::useSource() const
{
    return m_useSource;
}

void TargetSingleDeviceField::setUseSource(bool value)
{
    if (m_useSource == value)
        return;

    m_useSource = value;
    emit useSourceChanged();
}

QVariant TargetSingleDeviceField::build(const AggregatedEventPtr& event) const
{
    if (m_useSource)
    {
        const auto deviceIds = utils::getDeviceIds(event);

        // Use single camera (the last one). Synced with the old engine.
        if (NX_ASSERT(!deviceIds.isEmpty()))
            return QVariant::fromValue(deviceIds.last());

        return {};
    }

    return QVariant::fromValue(m_id);
}

TargetSingleDeviceFieldProperties TargetSingleDeviceField::properties() const
{
    return TargetSingleDeviceFieldProperties::fromVariantMap(descriptor()->properties);
}

QJsonObject TargetSingleDeviceField::openApiDescriptor()
{
    return utils::constructOpenApiDescriptor<TargetSingleDeviceField>();
}

} // namespace nx::vms::rules
