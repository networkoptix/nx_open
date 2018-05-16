#include "camera_thumbnail_cache.h"

#include <QtCore/QTimer>

#include <api/helpers/thumbnail_request_data.h>
#include <api/server_rest_connection.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <nx/client/core/utils/geometry.h>
#include <ui/style/skin.h>
#include <utils/common/delayed.h>

using namespace nx::client::desktop;

namespace {

static const QSize kDefaultThumbnailSize(0, 200);
static const qreal kDefaultAspectRatio = 4.0 / 3.0;
static const int kUpdateThumbnailsPeriodMs = 10 * 1000;
static const rest::Handle kInvalidHandle = 0;

} // namespace

/*
QnCameraThumbnailCache::ThumbnailData::ThumbnailData():
    status(Qn::ThumbnailStatus::Invalid),
    loadingHandle(kInvalidHandle)
{}*/

QnCameraThumbnailCache::QnCameraThumbnailCache(QObject* parent) :
    m_refreshingTimer(new QTimer(parent))
{
    m_refreshingTimer->setInterval(kUpdateThumbnailsPeriodMs);
    /* We are bad at signals, until we are a proper connector
    connect(m_refreshingTimer, &QTimer::timeout, this,
        &QnCameraThumbnailCache::forceRefreshThumbnails);
    connect(resourcePool(), &QnResourcePool::statusChanged, this,
        &QnCameraThumbnailCache::at_resPool_statusChanged);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnCameraThumbnailCache::at_resPool_resourceRemoved);
        */
    m_refreshingTimer->start();
}

QnCameraThumbnailCache::~QnCameraThumbnailCache()
{
}
/*
void QnCameraThumbnailCache::selectCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_selectedCamera == camera)
        return;
    m_selectedCamera = camera;

    if (!camera)
    {
        emit statusChanged(Qn::ThumbnailStatus::Invalid);
        emit sizeHintChanged(sizeHint());
        emit imageChanged(QImage());
        return;
    }

    ThumbnailData& data = m_thumbnailByCamera[camera];
    if (data.status == Qn::ThumbnailStatus::Invalid || data.status == Qn::ThumbnailStatus::NoData)
        refreshSelectedCamera();

    executeDelayedParented(
        [this, cameraId = camera->getId(), thumbnail = QPixmap::fromImage(data.thumbnail)]
        {
            emit thumbnailReady(cameraId, thumbnail);
        }, this);

    emit statusChanged(data.status);
    emit sizeHintChanged(sizeHint());
    emit imageChanged(data.thumbnail);
}
*/
void QnCameraThumbnailCache::refreshSelectedCamera(QnVirtualCameraResourcePtr m_selectedCamera)
{
    if (!m_selectedCamera)
        return;

    if (!isRequestAvailable(m_selectedCamera))
        return;

#ifdef FUCK_THIS
    ThumbnailData& data = m_thumbnailByCamera[m_selectedCamera];

    if (isUpdating(data.status))
        return;

    QSize defaultSize;
    data.loadingHandle = loadThumbnailForCamera(m_selectedCamera, defaultSize);
    data.status = data.loadingHandle != kInvalidHandle
        ? Qn::ThumbnailStatus::Refreshing
        : Qn::ThumbnailStatus::NoData;
#endif
}

bool QnCameraThumbnailCache::autoRefresh() const
{
    return m_refreshingTimer->isActive();
}

void QnCameraThumbnailCache::setAutoRefresh(bool value)
{
    if (autoRefresh() == value)
        return;

    if (value)
        m_refreshingTimer->start();
    else
        m_refreshingTimer->stop();
}

QSize QnCameraThumbnailCache::sizeHintForCamera(const QnVirtualCameraResourcePtr& camera,
    const QSize& limit)
{
    // TODO: #GDM process camera rotation?
    qreal aspectRatio = kDefaultAspectRatio;
    QSize tiling(1, 1);
    if (camera)
    {
        const auto cameraAr = camera->aspectRatio();
        tiling = camera->getVideoLayout()->size();
        if (cameraAr.isValid())
            aspectRatio = cameraAr.toFloat() * tiling.width() / tiling.height();
    }
    NX_ASSERT(!qFuzzyIsNull(aspectRatio));

    QSize result(limit);

    // Full-size image is requested
    if (result.width() <= 0 && result.height() <= 0)
    {
        if (!camera)
            return result;

        const auto stream = camera->streamInfo();
        result = nx::client::core::Geometry::cwiseMul(stream.getResolution(), tiling);
    }
    // Only height is given, calculating width by aspect ratio
    else if (result.width() <= 0)
    {
        result.setWidth(static_cast<int>(result.height() * aspectRatio));
    }
    // Only width is given, calculating height by aspect ratio
    else if (result.height() <= 0)
    {
        result.setHeight(static_cast<int>(result.width() / aspectRatio));
    }
    // If both width and height are set, aspect ratio is ignored.
    else
    {
        NX_ASSERT(!result.isEmpty());
    }
    return result;
}

Qn::ThumbnailStatus QnCameraThumbnailCache::statusForCamera(const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera)
        return Qn::ThumbnailStatus::Invalid;

    const auto iter = m_thumbnailByCamera.constFind(camera);
    if (iter == m_thumbnailByCamera.cend())
        return Qn::ThumbnailStatus::Invalid;

    return iter->status;
}

bool QnCameraThumbnailCache::hasThumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera)
        return false;

    const auto iter = m_thumbnailByCamera.constFind(camera);
    if (iter == m_thumbnailByCamera.cend())
        return false;

    return iter->status == Qn::ThumbnailStatus::Loaded
        || iter->status == Qn::ThumbnailStatus::Refreshing;
}

QImage QnCameraThumbnailCache::thumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera)
        return QImage();

    const auto iter = m_thumbnailByCamera.constFind(camera);
    if (iter == m_thumbnailByCamera.cend())
        return QImage();

    return iter->provider->image();
}
#ifdef FUCK_THIS
rest::Handle QnCameraThumbnailCache::loadThumbnailForCamera(const QnVirtualCameraResourcePtr& camera, QSize m_thumbnailSize)
{
    if (!camera || !camera->hasVideo(nullptr))
        return kInvalidHandle;

    nx::api::CameraImageRequest request;
    request.camera = camera;
    request.size = m_thumbnailSize;
    request.rotation = m_autoRotate
        ? nx::api::ImageRequest::kDefaultRotation
        : 0;

    if (!commonModule()->currentServer())
        return kInvalidHandle;
    // Fuck this, because we will have a separate set of image providers

    QPointer<QnCameraThumbnailCache> guard(this);
    return commonModule()->currentServer()->restConnection()->cameraThumbnailAsync(request,
        [guard, this, request] (bool success, rest::Handle id, const QByteArray& imageData)
        {
            if (!guard)
                return;

            if (!m_thumbnailByCamera.contains(request.camera))
                return;

            ThumbnailData &data = m_thumbnailByCamera[request.camera];
            if (data.loadingHandle != id)
                return;

            Qn::ThumbnailStatus oldStatus = data.status;

            data.loadingHandle = kInvalidHandle;
            data.status = Qn::ThumbnailStatus::NoData;

            if (success && !imageData.isEmpty())
            {
                QByteArray imageFormat = QnLexical::serialized(request.imageFormat).toUtf8();
                data.thumbnail.loadFromData(imageData, imageFormat);
                if (!data.thumbnail.isNull())
                    data.status = Qn::ThumbnailStatus::Loaded;
            }

            // We have no selected camera
            //if (request.camera == m_selectedCamera && oldStatus != data.status)
            //    emit statusChanged(data.status);

            if (data.status != Qn::ThumbnailStatus::Loaded)
                return;

            //if (request.camera == m_selectedCamera)
            //    emit imageChanged(data.thumbnail);

            QPixmap thumbnail = QPixmap::fromImage(data.thumbnail);

            emit thumbnailReady(request.camera->getId(), thumbnail);

        }, QThread::currentThread());

}
#endif

bool QnCameraThumbnailCache::isRequestAvailable(const QnVirtualCameraResourcePtr& camera) const
{
    switch (camera->getStatus())
    {
        case Qn::Online:
        case Qn::Recording:
            return true;

        default:
            return false;
    }
}

void QnCameraThumbnailCache::at_resPool_statusChanged(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (!m_thumbnailByCamera.contains(camera))
        return;

     auto& data = m_thumbnailByCamera[camera];

     if (!isUpdateRequired(camera, data.status))
         return;

     QSize defaultSize;
     // TODO: update request
     /*
     data.loadingHandle = loadThumbnailForCamera(camera, defaultSize);
     data.status = data.loadingHandle != kInvalidHandle
         ? Qn::ThumbnailStatus::Refreshing
         : Qn::ThumbnailStatus::NoData;
      */
}

void QnCameraThumbnailCache::at_resPool_resourceRemoved(const QnResourcePtr& resource)
{
    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        m_thumbnailByCamera.remove(camera);

    //if (m_selectedCamera == resource)
    //    selectCamera(QnVirtualCameraResourcePtr());
}

bool QnCameraThumbnailCache::isUpdating(Qn::ThumbnailStatus status) const
{
    return status == Qn::ThumbnailStatus::Loading || status == Qn::ThumbnailStatus::Refreshing;
}

bool QnCameraThumbnailCache::isUpdateRequired(const QnVirtualCameraResourcePtr& camera,
    Qn::ThumbnailStatus status) const
{
    switch (camera->getStatus())
    {
        case Qn::Recording:
            return !isUpdating(status);

        case Qn::Online:
            return (status == Qn::ThumbnailStatus::NoData);

        default:
            return false;
    }
}

void QnCameraThumbnailCache::forceRefreshThumbnails()
{
    for (auto it = m_thumbnailByCamera.begin(); it != m_thumbnailByCamera.end(); ++it)
    {
        if (!isUpdateRequired(it.key(), it->status))
            continue;

        // TODO: re-request thumbnails
        /*
        QSize thumbnailSize;
        it->loadingHandle = loadThumbnailForCamera(it.key(), thumbnailSize);

        it->status = it->loadingHandle != kInvalidHandle
            ? Qn::ThumbnailStatus::Refreshing
            : Qn::ThumbnailStatus::NoData;
         */
    }
}
