#include "camera_thumbnail_manager.h"

#include <QtCore/QTimer>

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
    m_refreshingTimer = new QTimer(this);
    m_refreshingTimer->setInterval(10 * 1000);

    connect(m_refreshingTimer,      &QTimer::timeout,                                   this,   &QnCameraThumbnailManager::forceRefreshThumbnails);
    connect(qnResPool,              &QnResourcePool::resourceRemoved,                   this,   &QnCameraThumbnailManager::at_resPool_resourceRemoved);
    connect(this,                   &QnCameraThumbnailManager::thumbnailReadyDelayed,   this,   &QnCameraThumbnailManager::thumbnailReady, Qt::QueuedConnection);
    setThumbnailSize(defaultThumbnailSize);

    m_refreshingTimer->start();
}

QnCameraThumbnailManager::~QnCameraThumbnailManager() {}

void QnCameraThumbnailManager::selectResource(const QnResourcePtr &resource) {
    ThumbnailData& data = m_thumbnailByResource[resource];
    if (data.status == None || data.status == Refreshing) {
        data.loadingHandle = loadThumbnailForResource(resource);
        if (data.loadingHandle != 0) {
            if (data.status != Refreshing)
                data.status = Loading;
        } else {
            data.status = NoSignal;
        }
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
    case Refreshing:
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

void QnCameraThumbnailManager::forceRefreshThumbnails() {
    for (auto it = m_thumbnailByResource.begin(); it != m_thumbnailByResource.end(); ++it) {
        if (it.key()->getStatus() == QnResource::Recording && it.value().status == Loaded)
            it.value().status = Refreshing;
    }
}
