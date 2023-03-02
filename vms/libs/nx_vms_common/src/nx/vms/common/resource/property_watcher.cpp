// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "property_watcher.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::common {

PropertyWatcher::PropertyWatcher(
    SystemContext* systemContext,
    std::set<QString> properties,
    ResourceFilter resourceFilter,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    m_propertiesToWatch(std::move(properties)),
    m_resourceFilter(std::move(resourceFilter))
{
    connect(systemContext->resourcePropertyDictionary(),
        &QnResourcePropertyDictionary::propertyChanged,
        this,
        &PropertyWatcher::handlePropertyChanged);
    connect(systemContext->resourcePropertyDictionary(),
        &QnResourcePropertyDictionary::propertyRemoved,
        this,
        &PropertyWatcher::handlePropertyChanged);
}

void PropertyWatcher::handlePropertyChanged(const QnUuid& resourceId, const QString& key)
{
    if (m_propertiesToWatch.find(key) == m_propertiesToWatch.cend())
        return;

    if (m_resourceFilter)
    {
        auto resource = resourcePool()->getResourceById(resourceId);
        if (!resource || !m_resourceFilter(resource))
            return;
    }

    emit propertyChanged();
}

} // namespace nx::vms::common
