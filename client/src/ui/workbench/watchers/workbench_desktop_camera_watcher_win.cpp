#include "workbench_desktop_camera_watcher_win.h"

#include <utils/common/checked_cast.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include "plugins/resource/avi/avi_resource.h"

#include <api/media_server_connection.h>
#include "core/resource/resource_fwd.h"
#include "plugins/resource/desktop_win/desktop_resource.h"

enum {
    ServerTimeUpdatePeriod = 1000 * 60 * 2, /* 2 minutes. */
};  


QnWorkbenchDesktopCameraWatcher::QnWorkbenchDesktopCameraWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);

}

QnWorkbenchDesktopCameraWatcher::~QnWorkbenchDesktopCameraWatcher() {
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceRemoved(resource);

    disconnect(resourcePool(), NULL, this, NULL);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) 
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    QnDesktopResourcePtr desktop = resource.dynamicCast<QnDesktopResource>();

    if(server) 
    {
        connect(server.data(), SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)), this, SLOT(at_server_serverIfFound(const QnMediaServerResourcePtr &)));
        connect(server.data(), SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));
        if (!server->getPrimaryIF().isEmpty()) {
            m_serverList << server;
            processServer(server);
        }
        else if (server->getStatus() == Qn::Online) {
            m_serverList << server;
            processServer(server);
        }
    }
    else if (desktop)
    {
        foreach(QnMediaServerResourcePtr server, m_serverList)
            processServer(server);
    }
}

void QnWorkbenchDesktopCameraWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) 
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    QnDesktopResourcePtr desktop = resource.dynamicCast<QnDesktopResource>();

    if(server) 
    {
        m_serverList.remove(server);
        disconnect(server.data(), NULL, this, NULL);

        QnDesktopResourcePtr desktop = qnResPool->getResourceById(QnDesktopResource::getDesktopResourceUuid()).dynamicCast<QnDesktopResource>();
        if (desktop)
            desktop->removeConnection(server);
    }
    else if (desktop)
    {
        foreach(QnMediaServerResourcePtr server, m_serverList)
            desktop->removeConnection(server);
    }

}

void QnWorkbenchDesktopCameraWatcher::at_server_serverIfFound(const QnMediaServerResourcePtr &server) 
{
    m_serverList << server;
    processServer(server);
}

void QnWorkbenchDesktopCameraWatcher::at_resource_statusChanged(const QnResourcePtr &resource) {
    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        processServer(server);
}

void QnWorkbenchDesktopCameraWatcher::processServer(QnMediaServerResourcePtr server)
{
    QnDesktopResourcePtr desktop = qnResPool->getResourceById(QnDesktopResource::getDesktopResourceUuid()).dynamicCast<QnDesktopResource>();
    if (desktop && m_serverList.contains(server)) 
    {
        if (server->getStatus() == Qn::Online)
            desktop->addConnection(server);
        else
            desktop->removeConnection(server);
    }
}

void QnWorkbenchDesktopCameraWatcher::at_recordingSettingsChanged()
{
    QnDesktopResourcePtr desktop = qnResPool->getResourceById(QnDesktopResource::getDesktopResourceUuid()).dynamicCast<QnDesktopResource>();
    if (!desktop)
        return;

    foreach (QnMediaServerResourcePtr mserver, m_serverList)
        desktop->removeConnection(mserver);
    foreach (QnMediaServerResourcePtr mserver, m_serverList)
        desktop->addConnection(mserver);
}
