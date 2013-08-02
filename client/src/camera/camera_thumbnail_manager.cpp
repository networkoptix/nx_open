#include "camera_thumbnail_manager.h"

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/style/skin.h>

QnCameraThumbnailManager::QnCameraThumbnailManager(QObject *parent) :
    QObject(parent)
{
    connect(qnResPool,  SIGNAL(resourceRemoved(QnResourcePtr)),         this,   SLOT(at_resPool_resourceRemoved(QnResourcePtr)));
    connect(this,       SIGNAL(thumbnailReadyDelayed(int,QPixmap)),     this,   SIGNAL(thumbnailReady(int,QPixmap)), Qt::QueuedConnection);
}

QnCameraThumbnailManager::~QnCameraThumbnailManager() {}

void QnCameraThumbnailManager::selectResource(const QnResourcePtr &resource) {
    ThumbnailData& data = m_thumbnailByResource[resource];
    if (data.status == None) {
        data.loadingHandle = loadThumbnailForResource(resource);
        if (data.loadingHandle != 0)
            data.status = Loading;
        else
            data.status = NoSignal;
    }

    QPixmap thumbnail;
    switch (data.status) {
    case None: //should never come here
        break;
    case Loading:
        thumbnail = qnSkin->pixmap("events/thumb_loading.png");
        break;
    case Loaded:
        thumbnail = QPixmap::fromImage(data.thumbnail);
        break;
    case NoData:
        thumbnail = qnSkin->pixmap("events/thumb_no_data.png");
        break;
    case NoSignal:
        thumbnail = qnSkin->pixmap("events/thumb_no_signal.png");
        break;
    }
    emit thumbnailReadyDelayed(resource->getId(), thumbnail);
}

int QnCameraThumbnailManager::loadThumbnailForResource(const QnResourcePtr &resource) {
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource)
        return 0;

    QnMediaServerResourcePtr serverResource = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(resource->getParentId()));
    if (!serverResource)
        return 0;

    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return 0;

    return serverConnection->getThumbnailAsync(
                resource.dynamicCast<QnNetworkResource>(),
                -1,
                QSize(0, 200),
                QLatin1String("png"),
                QnMediaServerConnection::IFrameAfterTime,
                this,
                SLOT(at_thumbnailReceived(int, const QImage&, int)));
}

void QnCameraThumbnailManager::at_thumbnailReceived(int status, const QImage &thumbnail, int handle) {
    foreach (QnResourcePtr resource, m_thumbnailByResource.keys()) {
        ThumbnailData &data = m_thumbnailByResource[resource];
        if (data.loadingHandle != handle)
            continue;

        if (status == 0) {
            data.thumbnail = thumbnail;
            data.status = Loaded;
        }
        else {
            data.status = NoData;
        }
        data.loadingHandle = 0;
        QPixmap thumbnail = data.status == Loaded
                ? QPixmap::fromImage(data.thumbnail)
                : qnSkin->pixmap("events/thumb_no_data.png");
        emit thumbnailReady(resource->getId(), thumbnail);
        break;
    }
}

void QnCameraThumbnailManager::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    m_thumbnailByResource.remove(resource);
}
