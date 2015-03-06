#include "desktop_camera_deleter.h"

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QTimer>

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

namespace {
    const int timeout = 60*1000;    //check once a minute
}

QnDesktopCameraDeleter::QnDesktopCameraDeleter(QObject *parent): QObject(parent) {
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        deleteQueuedResources();
        updateQueue();
    });
    timer->start(timeout);   
}

void QnDesktopCameraDeleter::deleteQueuedResources() {
    for (const QnResourcePtr &resource: m_queuedToDelete) {
        if (resource->getStatus() != Qn::Offline)
            continue;

        /* If the camera is placed on the layout, also remove the layout. */
        for (const QnLayoutResourcePtr &layout: qnResPool->getLayoutsWithResource(resource->getId())) {
            QnAppServerConnectionFactory::getConnection2()->getLayoutManager()->remove(layout->getId(), this, []{});
            qnResPool->removeResource(layout);
        }

        QnAppServerConnectionFactory::getConnection2()->getCameraManager()->remove(resource->getId(), this, []{});
        qnResPool->removeResource(resource);
    }
    m_queuedToDelete.clear();
}

void QnDesktopCameraDeleter::updateQueue() {
    
    QnResourceList desktopCameras = qnResPool->getResourcesWithFlag(Qn::desktop_camera);
    for(const QnResourcePtr &resource: desktopCameras) {
        if (resource->getStatus() == Qn::Offline)
            m_queuedToDelete << resource;
    }
}


#endif
