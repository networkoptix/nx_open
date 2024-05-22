// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_camera_connection_controller.h"

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

DesktopCameraConnectionController::DesktopCameraConnectionController(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
    // Server we are currently connected to MUST exist in the initial resources pool.
    connect(clientMessageProcessor(), &QnClientMessageProcessor::initialResourcesReceived,
        this, &DesktopCameraConnectionController::rememberCamera);

    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this, &DesktopCameraConnectionController::rememberCamera);

    connect(resourcePool(), &QnResourcePool::resourcesRemoved,
        this, &DesktopCameraConnectionController::atResourcesRemoved);

    connect(systemContext->userWatcher(), &core::UserWatcher::userChanged,
        this, &DesktopCameraConnectionController::atUserChanged);

    rememberCamera();
}

DesktopCameraConnectionController::~DesktopCameraConnectionController()
{
}

void DesktopCameraConnectionController::initialize()
{
    systemContext()->localSettings()->desktopCameraWasUsedAtPreviousLogin = true;
    initializeImpl();
}

void DesktopCameraConnectionController::reinitialize()
{
    m_desktop.reset();
    m_server.reset();
    initialize();
}

void DesktopCameraConnectionController::initializeImpl()
{
    rememberCamera();
    setServer(currentServer());
}

void DesktopCameraConnectionController::rememberCamera()
{
    if (!m_desktop)
    {
        m_desktop = resourcePool()->getResourceById<core::DesktopResource>(
            core::DesktopResource::getDesktopResourceUuid());
    }
}

void DesktopCameraConnectionController::disconnectCamera()
{
    if (m_desktop)
        m_desktop->disconnectFromServer();
    m_server.reset();
}

void DesktopCameraConnectionController::atResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (resource == m_desktop)
            m_desktop.reset();
        else if (resource == m_server)
            disconnectCamera();
    }
}

void DesktopCameraConnectionController::atUserChanged(const QnUserResourcePtr& user)
{
    disconnectCamera();

    if (!user)
        return;

    const auto userId = user->getId();
    auto localSettings = systemContext()->localSettings();
    if (!ini().enableDesktopCameraLazyInitialization
        || localSettings->desktopCameraWasUsedAtPreviousLogin())
    {
        initializeImpl();
        localSettings->desktopCameraWasUsedAtPreviousLogin = false;
    }
}

void DesktopCameraConnectionController::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server && m_desktop && m_desktop->isConnected())
        return;

    disconnectCamera();

    m_server = server;

    const auto user = systemContext()->userWatcher()->user();
    if (m_desktop && m_server && user)
        m_desktop->initializeConnection(m_server, user->getId());
}

} // namespace nx::vms::client::desktop
