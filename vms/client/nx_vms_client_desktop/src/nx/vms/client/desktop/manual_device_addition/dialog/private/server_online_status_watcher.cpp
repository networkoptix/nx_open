// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_online_status_watcher.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {

ServerOnlineStatusWatcher::ServerOnlineStatusWatcher(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(qnClientCoreModule->commonModule())
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (server && server == m_server)
                setServer(QnMediaServerResourcePtr());
        });
}

void ServerOnlineStatusWatcher::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    if (m_server)
        m_server->disconnect(this);

    m_server = server;

    if (!m_server)
        return;

    connect(m_server.get(), &QnMediaServerResource::statusChanged,
        this, &ServerOnlineStatusWatcher::statusChanged);

    emit statusChanged();
}

bool ServerOnlineStatusWatcher::isOnline() const
{
    return m_server && m_server->getStatus() == nx::vms::api::ResourceStatus::online;
}

} // namespace nx::vms::client::desktop
