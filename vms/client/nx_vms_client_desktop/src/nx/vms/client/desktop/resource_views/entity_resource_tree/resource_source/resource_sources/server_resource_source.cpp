// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_resource_source.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

ServerResourceSource::ServerResourceSource(const QnResourcePool* resourcePool)
    :
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::server))
                return;

            const auto server = resource.staticCast<QnMediaServerResource>();
            emit resourceAdded(resource);
        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::server))
                emit resourceRemoved(resource);
        });
}

QVector<QnResourcePtr> ServerResourceSource::getResources()
{
    if (!NX_ASSERT(m_resourcePool))
        return {};

    const auto servers = m_resourcePool->servers();
    return {servers.begin(), servers.end()};
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
