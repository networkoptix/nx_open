// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "current_system_servers.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_settings.h>

namespace {

bool sameServer(const QnMediaServerResourcePtr& first, const QnMediaServerResourcePtr& second)
{
    return first && second && first->getId() == second->getId();
}

using SameServerPredicate = std::function<bool (const QnMediaServerResourcePtr&)>;
SameServerPredicate makeSameServerPredicate(const QnMediaServerResourcePtr& server)
{
    return
        [server](const QnMediaServerResourcePtr& other)
        {
            return sameServer(server, other);
        };
}

bool contains(const QnMediaServerResourceList& servers, const QnMediaServerResourcePtr& server)
{
    return std::any_of(servers.begin(), servers.end(), makeSameServerPredicate(server));
}

} // namespace

namespace nx::vms::client::desktop {

CurrentSystemServers::CurrentSystemServers(QObject* parent):
    base_type(parent)
{
    const auto pool = resourcePool();
    connect(pool, &QnResourcePool::resourceAdded, this, &CurrentSystemServers::tryAddServer);
    connect(pool, &QnResourcePool::resourceRemoved, this, &CurrentSystemServers::tryRemoveServer);

    for (const auto server: pool->servers())
        tryAddServer(server);
}

QnMediaServerResourceList CurrentSystemServers::servers() const
{
    return m_servers;
}

int CurrentSystemServers::serversCount() const
{
    return m_servers.size();
}

void CurrentSystemServers::tryAddServer(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server->hasFlags(Qn::fake_server))
        return;

    if (contains(m_servers, server))
        return;

    m_servers.append(server);

    emit serverAdded(server);
    emit serversCountChanged();
}

void CurrentSystemServers::tryRemoveServer(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    const auto itServer = std::find_if(m_servers.begin(), m_servers.end(),
        makeSameServerPredicate(server));

    if (itServer == m_servers.end())
        return;

    (*itServer)->disconnect(this);
    m_servers.erase(itServer);

    emit serverRemoved(server->getId());
    emit serversCountChanged();
}

} // namespace nx::vms::client::desktop
