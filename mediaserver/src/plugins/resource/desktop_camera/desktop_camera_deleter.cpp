#include "desktop_camera_deleter.h"

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QTimer>

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>

namespace {
    const int timeout = 60*1000;    //check once a minute
}

QnDesktopCameraDeleter::QnDesktopCameraDeleter(QObject *parent): QObject(parent) {
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {

        // remove resources that have been offline for more than one iteration
        for (const QnResourcePtr &resource: m_queuedToDelete) {
            if (resource->getStatus() != Qn::Offline)
                continue;
            QnAppServerConnectionFactory::getConnection2()->getCameraManager()->remove(resource->getId(), this, []{});
            qnResPool->removeResource(resource);
        }
        m_queuedToDelete.clear();

        // queue to removing all resources that went offline since the last check
        QnResourceList desktopCameras = qnResPool->getResourcesWithFlag(Qn::desktop_camera);
        for(const QnResourcePtr &resource: desktopCameras) {
            if (resource->getStatus() == Qn::Offline)
                m_queuedToDelete << resource;
        }
    });
    timer->start(timeout);   
}

#endif
