#include "workbench_desktop_camera_watcher.h"

#include <utils/common/checked_cast.h>

#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <plugins/resource/desktop_camera/desktop_resource_base.h>
#include "client/client_runtime_settings.h"

QnWorkbenchDesktopCameraWatcher::QnWorkbenchDesktopCameraWatcher(QObject *parent):
    QObject(parent),
    QnSessionAwareDelegate(parent),
    m_desktopConnected(false)
{
    /* Server we are currently connected to MUST exist in the initial resources pool. */
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::initialResourcesReceived,    this,   &QnWorkbenchDesktopCameraWatcher::initialize);

    connect(qnResPool, &QnResourcePool::resourceAdded,   this,   &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,   &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved);

    initialize();
}

QnWorkbenchDesktopCameraWatcher::~QnWorkbenchDesktopCameraWatcher() {
    setServer(QnMediaServerResourcePtr());
    disconnect(qnResPool, NULL, this, NULL);
}

void QnWorkbenchDesktopCameraWatcher::initialize() {
    setServer(qnCommon->currentServer());
}

void QnWorkbenchDesktopCameraWatcher::deinitialize() {
    setServer(QnMediaServerResourcePtr());
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if ((m_desktop && m_desktopConnected) || qnRuntime->isVideoWallMode() || qnRuntime->isActiveXMode())
        return;
    if (QnDesktopResourcePtr desktop = resource.dynamicCast<QnDesktopResource>()) {
        m_desktop = desktop;
        initialize();
    }
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    QnDesktopResourcePtr desktop = resource.dynamicCast<QnDesktopResource>();

    if (server && m_server == server) {
        deinitialize();
    }
    else if (desktop && m_desktop == desktop) {
        deinitialize();
        m_desktop = QnDesktopResourcePtr();
    }
}

bool QnWorkbenchDesktopCameraWatcher::tryClose(bool force) {
    if (force)
        deinitialize();
    return true;
}

void QnWorkbenchDesktopCameraWatcher::forcedUpdate() {
    if (!m_desktop || !m_server)
        return;

    m_desktop->removeConnection(m_server);
    m_desktop->addConnection(m_server);
}

void QnWorkbenchDesktopCameraWatcher::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server && m_desktopConnected )
        return;

    if (m_desktop && m_server)
    {
        m_desktop->removeConnection(m_server);
        m_desktopConnected = false;
    }

    if (m_server != server)
        m_server = server;

    if (m_desktop && m_server)
    {
        m_desktop->addConnection(m_server);
        m_desktopConnected = true;
    }
}
