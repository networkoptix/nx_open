// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_audio_connection_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/screen_recording/audio_only/desktop_audio_only_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>

using namespace nx::vms::client;

namespace nx::client::mobile {

ServerAudioConnectionWatcher::ServerAudioConnectionWatcher(
    nx::vms::client::core::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext)
{
    const auto userWatcher = systemContext->userWatcher();

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &ServerAudioConnectionWatcher::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &ServerAudioConnectionWatcher::handleResourceRemoved);
    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
        this, &ServerAudioConnectionWatcher::tryAddCurrentServerConnection);
    connect(systemContext, &nx::vms::client::core::SystemContext::remoteIdChanged,
        this, &ServerAudioConnectionWatcher::handleRemoteIdChanged);

    handleRemoteIdChanged();
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
    if (const auto serverResource = resource.dynamicCast<QnMediaServerResource>())
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
    if (const auto serverResource = resource.dynamicCast<QnMediaServerResource>())
    {
        if (serverResource->getId() == m_remoteServerId)
        {
            tryRemoveCurrentServerConnection();
            m_server.reset();
        }
    }
}

void ServerAudioConnectionWatcher::updateLegacyDesktopResource()
{
    // Servers 6.0 and before receive two-way audio through the desktop camera connection.
    const bool isRequired = !m_remoteServerId.isNull()
        && systemContext()->moduleInformation().version < nx::utils::SoftwareVersion(6, 1);

    if (isRequired && !m_desktop)
    {
        m_desktop.reset(new vms::client::core::DesktopAudioOnlyResource());
        resourcePool()->addResource(m_desktop);
    }
    else if (!isRequired && m_desktop)
    {
        resourcePool()->removeResource(m_desktop);
        m_desktop.reset();
    }
}

void ServerAudioConnectionWatcher::handleRemoteIdChanged()
{
    tryRemoveCurrentServerConnection();
    m_server.reset();

    m_remoteServerId = systemContext()->currentServerId();
    updateLegacyDesktopResource();

    if (m_remoteServerId.isNull())
        return;

    handleResourceAdded(resourcePool()->getResourceById<QnMediaServerResource>(m_remoteServerId));
}

} // namespace nx::client::mobile
