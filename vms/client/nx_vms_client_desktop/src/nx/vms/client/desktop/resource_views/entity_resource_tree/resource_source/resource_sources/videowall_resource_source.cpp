// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_resource_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/webpage_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

VideowallResourceSource::VideowallResourceSource(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::videowall))
                emit resourceAdded(resource);
        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::videowall))
                emit resourceRemoved(resource);
        });
}

QVector<QnResourcePtr> VideowallResourceSource::getResources()
{
    return m_resourcePool->getResourcesWithFlag(Qn::videowall).toVector();
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
