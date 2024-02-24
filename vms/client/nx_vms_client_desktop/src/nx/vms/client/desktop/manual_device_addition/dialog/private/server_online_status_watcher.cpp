// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_online_status_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {

ServerOnlineStatusWatcher::ServerOnlineStatusWatcher(QObject* parent):
    base_type(parent)
{
}

void ServerOnlineStatusWatcher::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_connections.reset();

    m_server = server;

    if (!m_server)
        return;

    m_connections << connect(m_server.get(), &QnMediaServerResource::statusChanged,
        this, &ServerOnlineStatusWatcher::statusChanged);

    if (auto resourcePool = m_server->resourcePool(); NX_ASSERT(resourcePool))
    {
        m_connections << connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
            [this](const QnResourceList& resources)
            {
                if (resources.contains(m_server))
                    setServer(QnMediaServerResourcePtr());
            });
    }

    emit statusChanged();
}

bool ServerOnlineStatusWatcher::isOnline() const
{
    return m_server && m_server->getStatus() == nx::vms::api::ResourceStatus::online;
}

} // namespace nx::vms::client::desktop
