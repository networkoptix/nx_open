// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_desktop_camera_watcher.h"

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client;

QnWorkbenchDesktopCameraWatcher::QnWorkbenchDesktopCameraWatcher(QObject *parent):
    QObject(parent),
    QnSessionAwareDelegate(parent)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened,
        this,
        [this]()
        {
            nx::vms::client::core::RemoteConnectionAware connectionAccessor;
            if (NX_ASSERT(connectionAccessor.session()))
                connect(connectionAccessor.session().get(),
                    &nx::vms::client::core::RemoteSession::credentialsChanged,
                    this,
                    &QnWorkbenchDesktopCameraWatcher::forcedUpdate);
        });

    // Server we are currently connected to MUST exist in the initial resources pool.
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnWorkbenchDesktopCameraWatcher::initialize);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchDesktopCameraWatcher::forcedUpdate);

    connect(qnClientCoreModule->networkModule(),
        &nx::vms::client::core::NetworkModule::remoteIdChanged,
        this,
        &QnWorkbenchDesktopCameraWatcher::forcedUpdate);

    initialize();
}

QnWorkbenchDesktopCameraWatcher::~QnWorkbenchDesktopCameraWatcher()
{
}

void QnWorkbenchDesktopCameraWatcher::initialize()
{
    if (!m_desktop)
    {
        m_desktop = resourcePool()->getResourceById<core::DesktopResource>(
            core::DesktopResource::getDesktopResourceUuid());
    }

    setServer(currentServer());
}

void QnWorkbenchDesktopCameraWatcher::deinitialize()
{
    if (m_desktop)
        m_desktop->disconnectFromServer();
    m_server.reset();
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    if (resource->getId() == core::DesktopResource::getDesktopResourceUuid())
        initialize();
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved(
    const QnResourcePtr& resource)
{
    if (resource == m_desktop)
        m_desktop.reset();
    else if (resource == m_server)
        deinitialize();
}

bool QnWorkbenchDesktopCameraWatcher::tryClose(bool force)
{
    if (force)
        deinitialize();

    return true;
}

void QnWorkbenchDesktopCameraWatcher::forcedUpdate()
{
    deinitialize();
    initialize();
}

void QnWorkbenchDesktopCameraWatcher::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server && m_desktop && m_desktop->isConnected())
        return;

    deinitialize();

    m_server = server;
    const auto user = context()->user();

    if (m_desktop && m_server && user)
        m_desktop->initializeConnection(m_server, user->getId());
}
