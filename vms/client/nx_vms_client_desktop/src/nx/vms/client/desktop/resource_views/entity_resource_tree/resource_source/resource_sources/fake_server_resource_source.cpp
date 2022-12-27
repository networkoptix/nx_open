// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fake_server_resource_source.h"

#include <core/resource/fake_media_server.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

namespace {

// Returns whether the resource is a fake server and not belongs to the current system.
bool isTargetFakeServer(const QnResourcePtr& resource)
{
    if(const auto fakeServer = resource.dynamicCast<QnFakeMediaServerResource>())
        return !helpers::serverBelongsToCurrentSystem(fakeServer);

    return false;
}

} // namespace

FakeServerResourceSource::FakeServerResourceSource(
    const QnResourcePool* resourcePool)
    :
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (isTargetFakeServer(resource))
                    emit resourceAdded(resource);
            }
        });

    connect(m_resourcePool, &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (isTargetFakeServer(resource))
                    emit resourceRemoved(resource);
            }
        });
}

QVector<QnResourcePtr> FakeServerResourceSource::getResources()
{
    if (!NX_ASSERT(m_resourcePool))
        return QVector<QnResourcePtr>();

    const auto incompatibleServers = m_resourcePool->getIncompatibleServers();

    QVector<QnResourcePtr> result;
    for (const auto& server: incompatibleServers)
    {
        if (isTargetFakeServer(server))
            result << server;
    }

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
