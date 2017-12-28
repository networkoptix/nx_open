#include "server_online_status_watcher.h"

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace desktop {

ServerOnlineStatusWatcher::ServerOnlineStatusWatcher(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    connect(commonModule()->resourcePool(), &QnResourcePool::resourceRemoved, this,
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

    connect(m_server, &QnMediaServerResource::statusChanged,
        this, &ServerOnlineStatusWatcher::statusChanged);

    emit statusChanged();
}

bool ServerOnlineStatusWatcher::isOnline() const
{
    return m_server && m_server->getStatus() == Qn::Online;
}

} // namespace desktop
} // namespace client
} // namespace nx

