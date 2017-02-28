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

#include <utils/common/delayed.h>

namespace {

static const QSize kDefaultThumbnailSize(0, 200);
static const qreal kDefaultAspectRatio = 4.0 / 3.0;
static const int kUpdateThumbnailsPeriodMs = 10 * 1000;
static const rest::Handle kInvalidHandle = 0;

} // namespace

QnCameraThumbnailManager::ThumbnailData::ThumbnailData():
    status(Qn::ThumbnailStatus::Invalid),
    loadingHandle(kInvalidHandle)
{
}


QnCameraThumbnailManager::QnCameraThumbnailManager(QObject* parent) :
    base_type(parent),
    m_thumnailSize(kDefaultThumbnailSize),
    m_refreshingTimer(new QTimer(this))
{
    m_refreshingTimer->setInterval(kUpdateThumbnailsPeriodMs);

    connect(m_refreshingTimer, &QTimer::timeout, this,
        &QnCameraThumbnailManager::forceRefreshThumbnails);
    connect(qnResPool, &QnResourcePool::statusChanged, this,
        &QnCameraThumbnailManager::at_resPool_statusChanged);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnCameraThumbnailManager::at_resPool_resourceRemoved);

    m_refreshingTimer->start();
}

QnCameraThumbnailManager::~QnCameraThumbnailManager()
{
}

QnVirtualCameraResourcePtr QnCameraThumbnailManager::selectedCamera() const
{
    return m_selectedCamera;
}

void QnCameraThumbnailManager::selectCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_selectedCamera == camera)
        return;
    m_selectedCamera = camera;

    if (!camera)
    {
        emit statusChanged(Qn::ThumbnailStatus::Invalid);
        emit imageChanged(QImage());
        emit sizeHintChanged(sizeHint());
        return;
    }

    ThumbnailData& data = m_thumbnailByCamera[camera];
    if (data.status == Qn::ThumbnailStatus::Invalid
        || data.status == Qn::ThumbnailStatus::Refreshing)
    {
        data.loadingHandle = loadThumbnailForCamera(camera);
        if (data.loadingHandle != kInvalidHandle)
        {
            if (data.status != Qn::ThumbnailStatus::Refreshing)
                data.status = Qn::ThumbnailStatus::Loading;
        }
        else
        {
            data.status = Qn::ThumbnailStatus::NoData;
        }
    }

    executeDelayedParented(
        [this, cameraId = camera->getId(), thumbnail = QPixmap::fromImage(data.thumbnail)]
        {
            emit thumbnailReady(cameraId, thumbnail);
        }, kDefaultDelay, this);

    emit statusChanged(data.status);
    emit imageChanged(data.thumbnail);
}

QSize QnCameraThumbnailManager::thumbnailSize() const
{
    return m_thumnailSize;
}

QPixmap QnCameraThumbnailManager::scaledPixmap(const QPixmap& pixmap) const
{
    /* Check if no scaling required. */
    if (m_thumnailSize.isNull())
        return pixmap;

    if (m_thumnailSize.width() == 0)
        return pixmap.scaledToHeight(m_thumnailSize.height(), Qt::SmoothTransformation);
    if (m_thumnailSize.height() == 0)
        return pixmap.scaledToWidth(m_thumnailSize.width(), Qt::SmoothTransformation);
    return pixmap.scaled(m_thumnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void QnCameraThumbnailManager::setThumbnailSize(const QSize &size)
{
    if (m_thumnailSize == size)
        return;

    m_thumnailSize = size;
    emit sizeHintChanged(sizeHint());
}

QImage QnCameraThumbnailManager::image() const
{
    if (!m_selectedCamera)
        return QImage();

    return m_thumbnailByCamera.value(m_selectedCamera).thumbnail;
}

QSize QnCameraThumbnailManager::sizeHint() const
{
    return sizeHintForCamera(m_selectedCamera, m_thumnailSize);
}

Qn::ThumbnailStatus QnCameraThumbnailManager::status() const
{
    if (!m_selectedCamera)
        return Qn::ThumbnailStatus::Invalid;

    return m_thumbnailByCamera.value(m_selectedCamera).status;
}

QSize QnCameraThumbnailManager::sizeHintForCamera(const QnVirtualCameraResourcePtr& camera,
    const QSize& limit)
{
    //TODO: #GDM process camera rotation?
    qreal aspectRatio = kDefaultAspectRatio;
    if (camera)
    {
        const auto cameraAr = camera->aspectRatio();
        if (cameraAr.isValid())
            aspectRatio = cameraAr.toFloat();
    }
    NX_ASSERT(!qFuzzyIsNull(aspectRatio));

    QSize result(limit);

    // Full-size image is requested
    if (result.width() <= 0 && result.height() <= 0)
    {
        if (!camera)
            return result;

        const auto stream = camera->defaultStream();
        result = stream.getResolution();
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

rest::Handle QnCameraThumbnailManager::loadThumbnailForCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || !camera->hasVideo(nullptr))
        return kInvalidHandle;

    QnThumbnailRequestData request;
    request.camera = camera;
    request.size = m_thumnailSize;
    request.imageFormat = QnThumbnailRequestData::JpgFormat;
    request.roundMethod = QnThumbnailRequestData::KeyFrameAfterMethod;
    request.format = Qn::SerializationFormat::UbjsonFormat;

    if (!qnCommon->currentServer())
        return kInvalidHandle;

    QPointer<QnCameraThumbnailManager> guard(this);
    return qnCommon->currentServer()->restConnection()->cameraThumbnailAsync(request,
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

            if (request.camera == m_selectedCamera && oldStatus != data.status)
                emit statusChanged(data.status);

            if (data.status != Qn::ThumbnailStatus::Loaded)
                return;

            if (request.camera == m_selectedCamera)
                emit imageChanged(data.thumbnail);

            QPixmap thumbnail = QPixmap::fromImage(data.thumbnail);
            emit thumbnailReady(request.camera->getId(), thumbnail);

        }, QThread::currentThread());
}

void QnCameraThumbnailManager::doLoadAsync()
{
    NX_ASSERT(false);
}

void QnCameraThumbnailManager::at_resPool_statusChanged(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (!m_thumbnailByCamera.contains(camera))
        return;

     ThumbnailData& data = m_thumbnailByCamera[camera];

     if (!isUpdateRequired(camera, data.status))
         return;

     data.loadingHandle = loadThumbnailForCamera(camera);
     data.status = data.loadingHandle != kInvalidHandle
         ? Qn::ThumbnailStatus::Refreshing
         : Qn::ThumbnailStatus::NoData;
}

void QnCameraThumbnailManager::at_resPool_resourceRemoved(const QnResourcePtr& resource)
{
    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        m_thumbnailByCamera.remove(camera);

    if (m_selectedCamera == resource)
        selectCamera(QnVirtualCameraResourcePtr());
}

bool QnCameraThumbnailManager::isUpdateRequired(const QnVirtualCameraResourcePtr& camera,
    Qn::ThumbnailStatus status) const
{
    switch (camera->getStatus())
    {
        case Qn::Recording:
            return (status != Qn::ThumbnailStatus::Loading) && (status != Qn::ThumbnailStatus::Refreshing);

        case Qn::Online:
            return (status == Qn::ThumbnailStatus::NoData);

        default:
            return false;
    }
}

void QnCameraThumbnailManager::forceRefreshThumbnails()
{
    for (auto it = m_thumbnailByCamera.begin(); it != m_thumbnailByCamera.end(); ++it)
    {
        if (!isUpdateRequired(it.key(), it->status))
            continue;

        it->loadingHandle = loadThumbnailForCamera(it.key());
        it->status = it->loadingHandle != kInvalidHandle
            ? Qn::ThumbnailStatus::Refreshing
            : Qn::ThumbnailStatus::NoData;
    }
}
