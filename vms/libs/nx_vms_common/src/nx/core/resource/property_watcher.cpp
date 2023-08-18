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

        connect(m_resourcePool, &QnResourcePool::resourcesAdded,
            this, &PropertyWatcher::at_resourcesAdded);

        connect(m_resourcePool, &QnResourcePool::resourcesRemoved,
            this, &PropertyWatcher::at_resourcesRemoved);

        QnResourceList resources = m_resourcePool->getResources(m_resourceFilter);

        for (const QnResourcePtr& resource: resources)
        {
            connect(resource.data(), &QnResource::propertyChanged,
                this, &PropertyWatcher::at_propertyChanged);
        }
    }

    emit propertyChanged();
}

void PropertyWatcher::at_resourcesAdded(const QnResourceList& resources)
{
    bool match = false;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_resourceFilter)
            return;

        for (const auto& resource: resources)
        {
            if (m_resourceFilter(resource))
            {
                connect(resource.data(), &QnResource::propertyChanged,
                    this, &PropertyWatcher::at_propertyChanged);
                match = true;
            }
        }
    }

    if (match)
        emit propertyChanged();
}

void PropertyWatcher::at_resourcesRemoved(const QnResourceList& resources)
{
    bool match = false;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_resourceFilter)
            return;

        for (const auto& resource: resources)
        {
            resource->disconnect(this);

            if (!match && m_resourceFilter(resource))
                match = true;
        }
    }

    if (match)
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
