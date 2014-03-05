#include "camera_thumbnail_manager.h"

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/style/skin.h>

namespace {
    QSize defaultThumbnailSize(0, 200);
}

QnCameraThumbnailManager::QnCameraThumbnailManager(QObject *parent) :
    QObject(parent),
    m_thumnailSize(0, 0)
{
    connect(qnResPool,  SIGNAL(resourceRemoved(QnResourcePtr)),         this,   SLOT(at_resPool_resourceRemoved(QnResourcePtr)));
    connect(this,       SIGNAL(thumbnailReadyDelayed(int,QPixmap)),     this,   SIGNAL(thumbnailReady(int,QPixmap)), Qt::QueuedConnection);
    setThumbnailSize(defaultThumbnailSize);
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
    case NoData:
    case NoSignal:
        thumbnail = m_statusPixmaps[data.status];
        break;
    case Loaded:
        thumbnail = QPixmap::fromImage(data.thumbnail);
        break;
    }
    emit thumbnailReadyDelayed(resource->getId(), thumbnail);
}

void QnCameraThumbnailManager::setThumbnailSize(const QSize &size) {
    if (m_thumnailSize == size)
        return;

    m_thumnailSize = size;
    m_statusPixmaps.clear();
    m_statusPixmaps[Loading] = qnSkin->pixmap("events/thumb_loading.png").scaled(m_thumnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_statusPixmaps[NoData] = qnSkin->pixmap("events/thumb_no_data.png").scaled(m_thumnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_statusPixmaps[NoSignal] = qnSkin->pixmap("events/thumb_no_signal.png").scaled(m_thumnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

int QnCameraThumbnailManager::loadThumbnailForResource(const QnResourcePtr &resource) {
    QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>();
    if (!networkResource)
        return 0;

    QnMediaServerResourcePtr serverResource = qnResPool->getResourceById(resource->getParentId()).dynamicCast<QnMediaServerResource>();
    if (!serverResource)
        return 0;

    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return 0;

    return serverConnection->getThumbnailAsync(
                networkResource,
                -1,
                m_thumnailSize,
                QLatin1String("jpg"),
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
