// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_camera_initializer.h"

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

DesktopCameraInitializer::DesktopCameraInitializer(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    // Server we are currently connected to MUST exist in the initial resources pool.
    connect(clientMessageProcessor(),
        &QnClientMessageProcessor::initialResourcesReceived,
        this,
        &DesktopCameraInitializer::initialize);

    connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
        &DesktopCameraInitializer::atResourcesAdded);

    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        &DesktopCameraInitializer::atResourcesRemoved);

    connect(systemContext->userWatcher(), &core::UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            deinitialize();
            if (user)
                initialize();
        });

    initialize();
}

DesktopCameraInitializer::~DesktopCameraInitializer()
{
}

void DesktopCameraInitializer::initialize()
{
    if (!m_desktop)
    {
        m_desktop = resourcePool()->getResourceById<core::DesktopResource>(
            core::DesktopResource::getDesktopResourceUuid());
    }

    setServer(currentServer());
}

void DesktopCameraInitializer::deinitialize()
{
    if (m_desktop)
        m_desktop->disconnectFromServer();
    m_server.reset();
}

void DesktopCameraInitializer::atResourcesAdded(const QnResourceList& /*resources*/)
{
    initialize();
}

void DesktopCameraInitializer::atResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (resource == m_desktop)
            m_desktop.reset();
        else if (resource == m_server)
            deinitialize();
    }
}

void DesktopCameraInitializer::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server && m_desktop && m_desktop->isConnected())
        return;

    deinitialize();

    m_server = server;
    const auto user = systemContext()->userWatcher()->user();

    if (m_desktop && m_server && user)
        m_desktop->initializeConnection(m_server, user->getId());
}

} // namespace nx::vms::client::desktop
