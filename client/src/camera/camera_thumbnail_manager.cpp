#include "camera_thumbnail_manager.h"

#include <QtCore/QTimer>

#include <api/helpers/thumbnail_request_data.h>
#include <api/server_rest_connection.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <ui/style/skin.h>

namespace {

    const QSize kDefaultThumbnailSize(0, 200);

    const int kUpdateThumbnailsPeriodMs = 10 * 1000;

    const rest::Handle kInvalidHandle = 0;
}

QnCameraThumbnailManager::ThumbnailData::ThumbnailData()
    : status(None)
    , loadingHandle(kInvalidHandle)
{}


QnCameraThumbnailManager::QnCameraThumbnailManager(QObject *parent)
    : QObject(parent)
    , m_thumnailSize(kDefaultThumbnailSize)
    , m_refreshingTimer(new QTimer(this))

{
    m_refreshingTimer->setInterval(kUpdateThumbnailsPeriodMs);

    connect(m_refreshingTimer,      &QTimer::timeout,                                   this,   &QnCameraThumbnailManager::forceRefreshThumbnails);
    connect(qnResPool,              &QnResourcePool::statusChanged,                     this,   &QnCameraThumbnailManager::at_resPool_statusChanged);
    connect(qnResPool,              &QnResourcePool::resourceRemoved,                   this,   &QnCameraThumbnailManager::at_resPool_resourceRemoved);
    connect(this,                   &QnCameraThumbnailManager::thumbnailReadyDelayed,   this,   &QnCameraThumbnailManager::thumbnailReady, Qt::QueuedConnection);

    m_refreshingTimer->start();
    updateStatusPixmaps();
}

QnCameraThumbnailManager::~QnCameraThumbnailManager() {}

void QnCameraThumbnailManager::selectResource(const QnVirtualCameraResourcePtr &camera) {
    ThumbnailData& data = m_thumbnailByCamera[camera];
    if (data.status == None || data.status == Refreshing) {
        data.loadingHandle = loadThumbnailForCamera(camera);
        if (data.loadingHandle != kInvalidHandle) {
            if (data.status != Refreshing)
                data.status = Loading;
        } else {
            data.status = NoData;
        }
    }

    QPixmap thumbnail;
    switch (data.status) {
    case None: //should never come here
        break;
    case Loading:
    case NoData:
        thumbnail = m_statusPixmaps[data.status];
        break;
    case Loaded:
    case Refreshing:
        thumbnail = QPixmap::fromImage(data.thumbnail);
        break;
    }
    emit thumbnailReadyDelayed(camera->getId(), thumbnail);
}

QSize QnCameraThumbnailManager::thumbnailSize() const
{
    return m_thumnailSize;
}

QPixmap QnCameraThumbnailManager::statusPixmap(ThumbnailStatus status) {
    return m_statusPixmaps[status];
}

QPixmap QnCameraThumbnailManager::scaledPixmap(const QPixmap &pixmap) const {
    /* Check if no scaling required. */
    if (m_thumnailSize.isNull())
        return pixmap;

    if (m_thumnailSize.width() == 0)
        return pixmap.scaledToHeight(m_thumnailSize.height(), Qt::SmoothTransformation);
    if (m_thumnailSize.height() == 0)
        return pixmap.scaledToWidth(m_thumnailSize.width(), Qt::SmoothTransformation);
    return pixmap.scaled(m_thumnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void QnCameraThumbnailManager::updateStatusPixmaps()
{
    m_statusPixmaps.clear();

    m_statusPixmaps[Loading] = scaledPixmap(qnSkin->pixmap("events/thumb_loading.png"));
    m_statusPixmaps[NoData] = scaledPixmap(qnSkin->pixmap("events/thumb_no_data.png"));
}


void QnCameraThumbnailManager::setThumbnailSize(const QSize &size)
{
    if (m_thumnailSize == size)
        return;

    m_thumnailSize = size;
    updateStatusPixmaps();
}

rest::Handle QnCameraThumbnailManager::loadThumbnailForCamera(const QnVirtualCameraResourcePtr &camera) {

    if (!camera || !camera->hasVideo(nullptr))
        return kInvalidHandle;

    QnThumbnailRequestData request;
    request.camera = camera;
    request.size = m_thumnailSize;
    request.imageFormat = QnThumbnailRequestData::JpgFormat;
    request.roundMethod = QnThumbnailRequestData::KeyFrameAfterMethod;

    if (!qnCommon->currentServer())
        return kInvalidHandle;

    QPointer<QnCameraThumbnailManager> guard(this);
    return qnCommon->currentServer()->restConnection()->cameraThumbnailAsync(request,  [guard, this, request] (bool success, rest::Handle id, const QByteArray &imageData)
    {
        if (!guard)
            return;

        if (!m_thumbnailByCamera.contains(request.camera))
            return;

        ThumbnailData &data = m_thumbnailByCamera[request.camera];
        if (data.loadingHandle != id)
            return;

        data.loadingHandle = kInvalidHandle;
        data.status = NoData;

        if (success && !imageData.isEmpty())
        {
            QByteArray imageFormat = QnLexical::serialized<QnThumbnailRequestData::ThumbnailFormat>(request.imageFormat).toUtf8();
            data.thumbnail.loadFromData(imageData, imageFormat);
            if (!data.thumbnail.isNull())
                data.status = Loaded;
        }

        QPixmap thumbnail = data.status == Loaded
            ? QPixmap::fromImage(data.thumbnail)
            : m_statusPixmaps[data.status];

        emit thumbnailReady(request.camera->getId(), thumbnail);
    }, QThread::currentThread());


}

void QnCameraThumbnailManager::at_resPool_statusChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (!m_thumbnailByCamera.contains(camera))
        return;

     ThumbnailData &data = m_thumbnailByCamera[camera];
     if (!isUpdateRequired(camera, data.status))
         return;

     data.loadingHandle = loadThumbnailForCamera(camera);
     data.status = data.loadingHandle != kInvalidHandle
         ? Refreshing
         : NoData;
}

void QnCameraThumbnailManager::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        m_thumbnailByCamera.remove(camera);
}

bool QnCameraThumbnailManager::isUpdateRequired(const QnVirtualCameraResourcePtr &camera, const ThumbnailStatus status) const {
    switch (camera->getStatus()) {
    case Qn::Recording:
        return (status != Loading) && (status != Refreshing);
    case Qn::Online:
        return (status == NoData);
    default:
        break;
    }
    return false;
}

void QnCameraThumbnailManager::forceRefreshThumbnails() {
    for (auto it = m_thumbnailByCamera.begin(); it != m_thumbnailByCamera.end(); ++it) {
        if (!isUpdateRequired(it.key(), it->status))
            continue;

        it->loadingHandle = loadThumbnailForCamera(it.key());
        it->status = it->loadingHandle != kInvalidHandle
            ? Refreshing
            : NoData;
    }
}
