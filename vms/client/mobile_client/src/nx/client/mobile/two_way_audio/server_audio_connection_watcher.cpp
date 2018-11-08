#include "server_audio_connection_watcher.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>

namespace nx {
namespace client {
namespace mobile {

ServerAudioConnectionWatcher::ServerAudioConnectionWatcher(QObject* parent)
{
    const auto common = commonModule();
    const auto pool = common->resourcePool();
    const auto userWatcher = common->instance<nx::vms::client::core::UserWatcher>();

    connect(pool, &QnResourcePool::resourceAdded,
        this, &ServerAudioConnectionWatcher::handleResourceAdded);
    connect(pool, &QnResourcePool::resourceRemoved,
        this, &ServerAudioConnectionWatcher::handleResourceRemoved);
    connect(common, &QnCommonModule::remoteIdChanged,
        this, &ServerAudioConnectionWatcher::handleRemoteIdChanged);
    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
        this, &ServerAudioConnectionWatcher::tryAddCurrentServerConnection);

    handleRemoteIdChanged();
    for (const auto resource: pool->getResources<QnDesktopResource>())
        handleResourceAdded(resource);
}

void ServerAudioConnectionWatcher::tryRemoveCurrentServerConnection()
{
    if (m_server && m_desktop)
        m_desktop->removeConnection(m_server);
}

void ServerAudioConnectionWatcher::tryAddCurrentServerConnection()
{
    tryRemoveCurrentServerConnection();

    const auto userWatcher = commonModule()->instance<nx::vms::client::core::UserWatcher>();
    const auto user = userWatcher->user();
    if (m_server && m_desktop && user)
        m_desktop->addConnection(m_server, user->getId());
}

void ServerAudioConnectionWatcher::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto desktopResource = resource.dynamicCast<QnDesktopResource>())
    {
        tryRemoveCurrentServerConnection();
        m_desktop = desktopResource;
        tryAddCurrentServerConnection();
    }
    else if (const auto serverResource = resource.dynamicCast<QnMediaServerResource>())
    {
        if (serverResource->getId() == m_remoteServerId)
        {
            m_server = serverResource;
            tryAddCurrentServerConnection();
        }
    }
}

void ServerAudioConnectionWatcher::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto desktopResource = resource.dynamicCast<QnDesktopResource>())
    {
        if (desktopResource != m_desktop)
            return;

        tryRemoveCurrentServerConnection();
        m_desktop.reset();
    }
    else if (const auto serverResource = resource.dynamicCast<QnMediaServerResource>())
    {
        if (serverResource->getId() == m_remoteServerId)
        {
            tryRemoveCurrentServerConnection();
            m_server.reset();
        }
    }
}

void ServerAudioConnectionWatcher::handleRemoteIdChanged()
{
    tryRemoveCurrentServerConnection();

    const auto common = commonModule();
    const auto pool = common->resourcePool();
    m_remoteServerId = common->remoteGUID();
    handleResourceAdded(pool->getResourceById<QnMediaServerResource>(m_remoteServerId));
}

} // namespace mobile
} // namespace client
} // namespace nx
