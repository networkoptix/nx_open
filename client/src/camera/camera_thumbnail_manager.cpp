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
    connect(qnResPool,              &QnResourcePool::statusChanged,                     this,   &QnCameraThumbnailManager::at_resPool_statusChanged);    
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


QPixmap QnCameraThumbnailManager::scaledPixmap(const QPixmap &pixmap) const {
    Q_ASSERT(!m_thumnailSize.isNull());
    if (m_thumnailSize.isNull())
        return pixmap;

    if (m_thumnailSize.width() == 0)
        return pixmap.scaledToHeight(m_thumnailSize.height(), Qt::SmoothTransformation);
    if (m_thumnailSize.height() == 0)
        return pixmap.scaledToWidth(m_thumnailSize.width(), Qt::SmoothTransformation);
    return pixmap.scaled(m_thumnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}


void QnCameraThumbnailManager::setThumbnailSize(const QSize &size) {
    if (m_thumnailSize == size)
        return;

    m_thumnailSize = size;
    m_statusPixmaps.clear();
    
    m_statusPixmaps[Loading] = scaledPixmap(qnSkin->pixmap("events/thumb_loading.png"));
    m_statusPixmaps[NoData] = scaledPixmap(qnSkin->pixmap("events/thumb_no_data.png"));
    m_statusPixmaps[NoSignal] = scaledPixmap(qnSkin->pixmap("events/thumb_no_signal.png"));
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

void QnCameraThumbnailManager::at_resPool_statusChanged(const QnResourcePtr &resource) {
    if (!m_thumbnailByResource.contains(resource))
        return;

     ThumbnailData &data = m_thumbnailByResource[resource];
     if (!isUpdateRequired(resource, data.status))
         return;

     data.status = Refreshing;
     data.loadingHandle = loadThumbnailForResource(resource);
}

void QnCameraThumbnailManager::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    m_thumbnailByResource.remove(resource);
}

bool QnCameraThumbnailManager::isUpdateRequired(const QnResourcePtr &resource, const ThumbnailStatus status) const {
    switch (resource->getStatus()) {
    case Qn::Recording:
        return (status != Loading) && (status != Refreshing);
    case Qn::Online:
        return (status == NoData) || (status == NoSignal);
    default:
        break;
    }
    return false;
}

void QnCameraThumbnailManager::forceRefreshThumbnails() {
    for (auto it = m_thumbnailByResource.begin(); it != m_thumbnailByResource.end(); ++it) {
        if (!isUpdateRequired(it.key(), it->status))
            continue;

        it->status = Refreshing;
        it->loadingHandle = loadThumbnailForResource(it.key());
    }
}
