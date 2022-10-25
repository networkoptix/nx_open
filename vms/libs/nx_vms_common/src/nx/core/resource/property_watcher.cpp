// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "property_watcher.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::core::resource {

PropertyWatcher::PropertyWatcher(QnResourcePool* resourcePool):
    m_resourcePool(resourcePool)
{
}

PropertyWatcher::~PropertyWatcher()
{
    disconnect(this);
}

void PropertyWatcher::watch(
    std::set<QString> properties,
    ResourceFilter resourceFilter)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        disconnect(this);

        m_propertiesToWatch = std::move(properties);
        m_resourceFilter = std::move(resourceFilter);

        connect(m_resourcePool, &QnResourcePool::resourceAdded,
            this, &PropertyWatcher::at_resourceAdded);

        connect(m_resourcePool, &QnResourcePool::resourceRemoved,
            this, &PropertyWatcher::at_resourceRemoved);

        QnResourceList resources = m_resourcePool->getResources(m_resourceFilter);

        for (const QnResourcePtr& resource: resources)
        {
            connect(resource.data(), &QnResource::propertyChanged,
                this, &PropertyWatcher::at_propertyChanged);
        }
    }

    emit propertyChanged();
}

void PropertyWatcher::at_resourceAdded(const QnResourcePtr& resource)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_resourceFilter && !m_resourceFilter(resource))
            return;

        connect(resource.data(), &QnResource::propertyChanged,
            this, &PropertyWatcher::at_propertyChanged);

    }

    emit propertyChanged();
}

void PropertyWatcher::at_resourceRemoved(const QnResourcePtr& resource)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_resourceFilter && !m_resourceFilter(resource))
            return;
    }

    emit propertyChanged();
}

void PropertyWatcher::at_propertyChanged(const QnResourcePtr& resource, const QString& propertyName)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_propertiesToWatch.find(propertyName) == m_propertiesToWatch.cend())
            return;
    }

    emit propertyChanged();
}

} // namespace nx::core::resource
