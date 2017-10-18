#include "workbench_desktop_camera_watcher.h"

#include <utils/common/checked_cast.h>

#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <plugins/resource/desktop_camera/desktop_resource_base.h>
#include "client/client_runtime_settings.h"
#include <ui/workbench/workbench_context.h>

QnWorkbenchDesktopCameraWatcher::QnWorkbenchDesktopCameraWatcher(QObject *parent):
    QObject(parent),
    QnSessionAwareDelegate(parent),
    m_desktopConnected(false)
{
    /* Server we are currently connected to MUST exist in the initial resources pool. */
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnWorkbenchDesktopCameraWatcher::initialize);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchDesktopCameraWatcher::forcedUpdate);

    initialize();
}

QnWorkbenchDesktopCameraWatcher::~QnWorkbenchDesktopCameraWatcher()
{
    setServer(QnMediaServerResourcePtr());
    disconnect(resourcePool(), NULL, this, NULL);
}

void QnWorkbenchDesktopCameraWatcher::initialize()
{
    setServer(commonModule()->currentServer());
}

void QnWorkbenchDesktopCameraWatcher::deinitialize()
{
    setServer(QnMediaServerResourcePtr());
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    if ((m_desktop && m_desktopConnected) || !qnRuntime->isDesktopMode())
        return;

    if (const auto desktop = resource.dynamicCast<QnDesktopResource>())
    {
        m_desktop = desktop;
        initialize();
    }
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved(
    const QnResourcePtr &resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    const auto desktop = resource.dynamicCast<QnDesktopResource>();

    if (server && m_server == server)
    {
        deinitialize();
    }
    else if (desktop && m_desktop == desktop)
    {
        deinitialize();
        m_desktop = QnDesktopResourcePtr();
    }
}

bool QnWorkbenchDesktopCameraWatcher::tryClose(bool force)
{
    if (force)
        deinitialize();
    return true;
}

void QnWorkbenchDesktopCameraWatcher::forcedUpdate()
{
    if (!m_desktop || !m_server)
        return;

    m_desktop->removeConnection(m_server);
    m_desktopConnected = false;

    const auto user = context()->user();
    if (user)
    {
        m_desktop->addConnection(m_server, user->getId());
        m_desktopConnected = true;
    }
}

void QnWorkbenchDesktopCameraWatcher::setServer(const QnMediaServerResourcePtr &server)
{
    if (m_server == server && m_desktopConnected)
        return;

    if (m_desktop && m_server)
    {
        m_desktop->removeConnection(m_server);
        m_desktopConnected = false;
    }

    if (m_server != server)
        m_server = server;

    const auto user = context()->user();

    if (m_desktop && m_server && user)
    {
        m_desktop->addConnection(m_server, user->getId());
        m_desktopConnected = true;
    }
}
