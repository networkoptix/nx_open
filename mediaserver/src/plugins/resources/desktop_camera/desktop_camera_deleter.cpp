#include "desktop_camera_deleter.h"

#include <QtCore/QTimer>

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>

namespace {
    const int timeout = 15*1000;    //check once a minute
}

QnDesktopCameraDeleter::QnDesktopCameraDeleter(QObject *parent): QObject(parent) {
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {

        // remove resources that have been offline for more than one iteration
        foreach (const QnResourcePtr &resource, m_queuedToDelete) {
            if (resource->getStatus() != QnResource::Offline)
                continue;
            QnAppServerConnectionFactory::getConnection2()->getCameraManager()->remove(resource->getId(), this, []{});
            qnResPool->removeResource(resource);
        }
        m_queuedToDelete.clear();

        // queue to removing all resources that went offline since the last check
        QnResourceList desktopCameras = qnResPool->getResourcesWithFlag(QnResource::desktop_camera);
        foreach(const QnResourcePtr &resource, desktopCameras) {
            if (resource->getStatus() == QnResource::Offline)
                m_queuedToDelete << resource;
        }

        qDebug() << "----resources----";
        foreach (const QnResourcePtr &resource, qnResPool->getResources()) {
            qDebug() << "resource name" << resource->getName() << "flags" << resource->flags();
        }

    });
    timer->start(timeout);   
}

