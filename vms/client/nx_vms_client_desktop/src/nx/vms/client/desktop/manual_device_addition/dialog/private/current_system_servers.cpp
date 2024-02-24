// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "current_system_servers.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {

CurrentSystemServers::CurrentSystemServers(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(),
        &QnResourcePool::resourcesAdded,
        this,
        &CurrentSystemServers::tryAddServers);
    connect(resourcePool(),
        &QnResourcePool::resourcesRemoved,
        this,
        &CurrentSystemServers::tryRemoveServers);
}

QnMediaServerResourceList CurrentSystemServers::servers() const
{
    return resourcePool()->servers();
}

int CurrentSystemServers::serversCount() const
{
    return servers().size();
}

void CurrentSystemServers::tryAddServers(const QnResourceList& resources)
{
    bool countChanged = false;
    for (const auto& resource: resources)
    {
        if (const auto server = resource.dynamicCast<QnMediaServerResource>())
        {
            emit serverAdded(server);
            countChanged = true;
        }
    }
    if (countChanged)
        emit serversCountChanged();
}

void CurrentSystemServers::tryRemoveServers(const QnResourceList& resources)
{
    bool countChanged = false;
    for (const auto& resource: resources)
    {
        if (const auto server = resource.dynamicCast<QnMediaServerResource>())
        {
            emit serverRemoved(server->getId());
            countChanged = true;
        }
    }
    if (countChanged)
        emit serversCountChanged();
}

} // namespace nx::vms::client::desktop
