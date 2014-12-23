#include "workbench_desktop_camera_watcher_win.h"

#include <utils/common/checked_cast.h>

#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <plugins/resource/desktop_win/desktop_resource.h>

QnWorkbenchDesktopCameraWatcher::QnWorkbenchDesktopCameraWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchStateDelegate(parent)
{
    /* Server we are currently connected to MUST exist in the initial resources pool. */
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::initialResourcesReceived,    this,   &QnWorkbenchDesktopCameraWatcher::initialize);

    connect(resourcePool(), &QnResourcePool::resourceAdded,   this,   &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,   &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved);

    initialize();
}

QnWorkbenchDesktopCameraWatcher::~QnWorkbenchDesktopCameraWatcher() {
    setServer(QnMediaServerResourcePtr());
    disconnect(resourcePool(), NULL, this, NULL);
}

void QnWorkbenchDesktopCameraWatcher::initialize() {
    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->remoteGUID()).dynamicCast<QnMediaServerResource>();
    setServer(server);
}

void QnWorkbenchDesktopCameraWatcher::deinitialize() {
    setServer(QnMediaServerResourcePtr());
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if (m_desktop)
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
    if (m_server == server)
        return;

    if (m_desktop && m_server)
        m_desktop->removeConnection(m_server);

    m_server = server;

    if (m_desktop && m_server)
        m_desktop->addConnection(m_server);
}
