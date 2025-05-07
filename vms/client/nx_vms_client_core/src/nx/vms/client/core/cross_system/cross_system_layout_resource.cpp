// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_resource.h"

#include <nx/vms/api/data/resource_property_key.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_data.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>

namespace nx::vms::client::core {

CrossSystemLayoutResource::CrossSystemLayoutResource()
{
    addFlags(Qn::cross_system);
}

void CrossSystemLayoutResource::update(const core::CrossSystemLayoutData& layoutData)
{
    if (!NX_ASSERT(layoutData.id == this->getId()))
        return;

    CrossSystemLayoutResourcePtr copy(new CrossSystemLayoutResource());
    core::fromDataToResource(layoutData, copy);
    copy->setFlags(flags()); //< Do not update current resource flags.
    QnResource::update(copy);

    setProperty(
        api::resource_properties::kCustomGroupIdPropertyKey,
        layoutData.customGroupId);
}

QString CrossSystemLayoutResource::getProperty(const QString& key) const
{
    if (key == api::resource_properties::kCustomGroupIdPropertyKey)
        return m_customGroupId;

    return base_type::getProperty(key);
}

bool CrossSystemLayoutResource::setProperty(
    const QString& key,
    const QString& value,
    bool markDirty)
{
    if (key == api::resource_properties::kCustomGroupIdPropertyKey)
    {
        if (value.toLower() == m_customGroupId.toLower())
            return false;

        const auto previousValue = m_customGroupId;
        m_customGroupId = value;

        emitPropertyChanged(key, previousValue, m_customGroupId);

        return true;
    }

    return base_type::setProperty(key, value, markDirty);
}

void CrossSystemLayoutResource::makeSystemConnectionsWithUserInteraction()
{
    QSet<QString> systemIds;

    const auto items = getItems();
    for (const auto& item: items)
    {
        const auto systemId = crossSystemResourceSystemId(item.resource);
        if (!systemId.isEmpty())
            systemIds.insert(systemId);
    }

    const auto manager = appContext()->cloudCrossSystemManager();
    for (const auto& systemId: systemIds)
    {
        if (auto crossSystem = manager->systemContext(systemId))
            crossSystem->initializeConnectionWithUserInteraction();
    }
}

LayoutResourcePtr CrossSystemLayoutResource::create() const
{
    return LayoutResourcePtr(new CrossSystemLayoutResource());
}

bool CrossSystemLayoutResource::doSaveAsync(SaveLayoutResultFunction callback)
{
    const auto thisPtr = toSharedPointer(this);
    appContext()->cloudLayoutsManager()->saveLayout(thisPtr,
        [layout = thisPtr, callback](bool success) { callback(success, layout); });
    return true;
}

} // namespace nx::vms::client::desktop
