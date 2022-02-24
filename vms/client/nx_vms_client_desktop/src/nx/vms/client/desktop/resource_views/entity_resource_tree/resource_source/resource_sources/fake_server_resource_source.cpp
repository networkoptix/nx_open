// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fake_server_resource_source.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

FakeServerResourceSource::FakeServerResourceSource(
    const QnResourcePool* resourcePool)
    :
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::server) && resource->hasFlags(Qn::fake))
                emit resourceAdded(resource);
        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::server) && resource->hasFlags(Qn::fake))
                emit resourceRemoved(resource);
        });
}

QVector<QnResourcePtr> FakeServerResourceSource::getResources()
{
    if (!NX_ASSERT(m_resourcePool))
        return QVector<QnResourcePtr>();

    const auto incompatibleServers = m_resourcePool->getIncompatibleServers();

    QVector<QnResourcePtr> result;
    std::transform(std::cbegin(incompatibleServers), std::cend(incompatibleServers),
        std::back_inserter(result),
        [](const QnMediaServerResourcePtr& server)
        {
            return server.staticCast<QnResource>();
        });

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
