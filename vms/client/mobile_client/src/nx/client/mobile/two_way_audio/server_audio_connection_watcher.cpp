// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_audio_connection_watcher.h"

#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/session/session_manager.h>

using namespace nx::vms::client;

namespace nx {
namespace client {
namespace mobile {

ServerAudioConnectionWatcher::ServerAudioConnectionWatcher(
    nx::vms::client::mobile::SessionManager* sessionManager,
    nx::vms::client::core::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    m_sessionManager(sessionManager)
{
    const auto userWatcher = systemContext->userWatcher();

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &ServerAudioConnectionWatcher::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &ServerAudioConnectionWatcher::handleResourceRemoved);
    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
        this, &ServerAudioConnectionWatcher::tryAddCurrentServerConnection);
    connect(m_sessionManager, &nx::vms::client::mobile::SessionManager::hasConnectedSessionChanged,
        this, &ServerAudioConnectionWatcher::handleConnectedChanged);

    handleConnectedChanged();
    for (const auto resource: resourcePool()->getResources<nx::vms::client::core::DesktopResource>())
        handleResourceAdded(resource);
}

void ServerAudioConnectionWatcher::tryRemoveCurrentServerConnection()
{
    if (m_server && m_desktop)
        m_desktop->disconnectFromServer();
}

void ServerAudioConnectionWatcher::tryAddCurrentServerConnection()
{
    tryRemoveCurrentServerConnection();

    const auto userWatcher = systemContext()->userWatcher();
    const auto user = userWatcher->user();
    if (m_server && m_desktop && user)
        m_desktop->initializeConnection(m_server, user->getId());
}

void ServerAudioConnectionWatcher::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto desktopResource = resource.dynamicCast<nx::vms::client::core::DesktopResource>())
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
    if (const auto desktopResource = resource.dynamicCast<nx::vms::client::core::DesktopResource>())
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

void ServerAudioConnectionWatcher::handleConnectedChanged()
{
    tryRemoveCurrentServerConnection();

    if (!m_sessionManager->hasConnectedSession())
        return;
    m_remoteServerId = systemContext()->currentServerId();
    handleResourceAdded(resourcePool()->getResourceById<QnMediaServerResource>(m_remoteServerId));
}

} // namespace mobile
} // namespace client
} // namespace nx
