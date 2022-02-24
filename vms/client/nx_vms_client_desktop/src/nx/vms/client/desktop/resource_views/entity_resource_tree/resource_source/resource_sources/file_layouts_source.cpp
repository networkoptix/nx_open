// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_layouts_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/file_layout_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

FileLayoutsSource::FileLayoutsSource(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::exported_layout))
            {
                connect(resource.get(), &QnResource::statusChanged, this,
                    [this](const QnResourcePtr& resource)
                    {
                        if (resource->isOnline())
                            emit resourceAdded(resource);
                        else
                            emit resourceRemoved(resource);
                    });

                if (resource->isOnline())
                    emit resourceAdded(resource);
            }
        });

    connect(resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::exported_layout))
            {
                resource->disconnect(this);
                emit resourceRemoved(resource);
            }
        });
}

QVector<QnResourcePtr> FileLayoutsSource::getResources()
{
    const auto exportedLayouts = m_resourcePool->getResourcesWithFlag(Qn::exported_layout).filtered(
        [](const QnResourcePtr& resource) { return resource->isOnline(); });

    QVector<QnResourcePtr> result;
    std::copy(std::cbegin(exportedLayouts), std::cend(exportedLayouts), std::back_inserter(result));
    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
