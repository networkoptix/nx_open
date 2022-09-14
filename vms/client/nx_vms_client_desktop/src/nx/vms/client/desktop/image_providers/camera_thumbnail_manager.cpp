// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_thumbnail_manager.h"

#include <QtCore/QTimer>

#include <api/helpers/thumbnail_request_data.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/common/delayed.h>

using nx::vms::client::core::Geometry;

namespace {

static const QSize kDefaultThumbnailSize(0, 200);
static const qreal kDefaultAspectRatio = 4.0 / 3.0;
static const int kUpdateThumbnailsPeriodMs = 10 * 1000;
static const rest::Handle kInvalidHandle = 0;

} // namespace

namespace nx::vms::client::desktop {

CameraThumbnailManager::ThumbnailData::ThumbnailData():
    status(Qn::ThumbnailStatus::Invalid),
    loadingHandle(kInvalidHandle)
{
}

CameraThumbnailManager::CameraThumbnailManager(QObject* parent) :
    base_type(parent),
    m_thumbnailSize(kDefaultThumbnailSize),
    m_refreshingTimer(new QTimer(this))
{
    m_refreshingTimer->setInterval(kUpdateThumbnailsPeriodMs);

    connect(m_refreshingTimer, &QTimer::timeout, this,
        &CameraThumbnailManager::forceRefreshThumbnails);

    connect(appContext()->unifiedResourcePool(),
        &UnifiedResourcePool::resourcesRemoved,
        this,
        &CameraThumbnailManager::atResourcesRemoved);

    m_refreshingTimer->start();
}

CameraThumbnailManager::~CameraThumbnailManager()
{
}

QnVirtualCameraResourcePtr CameraThumbnailManager::selectedCamera() const
{
    return m_selectedCamera;
}

void CameraThumbnailManager::selectCamera(const QnVirtualCameraResourcePtr& camera)
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

    connect(camera.get(), &QnResource::statusChanged,
        this, &CameraThumbnailManager::atCameraStatusChanged,
        Qt::UniqueConnection);

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

void CameraThumbnailManager::refreshSelectedCamera()
{
    if (!m_selectedCamera)
        return;

    ThumbnailData& data = m_thumbnailByCamera[m_selectedCamera];

    if (!m_selectedCamera->isOnline())
    {
        data.status = Qn::ThumbnailStatus::NoData;
        return;
    }

    if (isUpdating(data.status))
        return;

    data.loadingHandle = loadThumbnailForCamera(m_selectedCamera);
    data.status = data.loadingHandle != kInvalidHandle
        ? Qn::ThumbnailStatus::Refreshing
        : Qn::ThumbnailStatus::NoData;
}

bool CameraThumbnailManager::autoRotate() const
{
    return m_autoRotate;
}

void CameraThumbnailManager::setAutoRotate(bool value)
{
    if (m_autoRotate == value)
        return;

    m_autoRotate = value;
    forceRefreshThumbnails();
}

bool CameraThumbnailManager::autoRefresh() const
{
    return m_refreshingTimer->isActive();
}

void CameraThumbnailManager::setAutoRefresh(bool value)
{
    if (autoRefresh() == value)
        return;

    if (value)
        m_refreshingTimer->start();
    else
        m_refreshingTimer->stop();
}

QSize CameraThumbnailManager::thumbnailSize() const
{
    return m_thumbnailSize;
}

QPixmap CameraThumbnailManager::scaledPixmap(const QPixmap& pixmap) const
{
    /* Check if no scaling required. */
    if (m_thumbnailSize.isNull())
        return pixmap;

    if (m_thumbnailSize.width() == 0)
        return pixmap.scaledToHeight(m_thumbnailSize.height(), Qt::SmoothTransformation);
    if (m_thumbnailSize.height() == 0)
        return pixmap.scaledToWidth(m_thumbnailSize.width(), Qt::SmoothTransformation);
    return pixmap.scaled(m_thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void CameraThumbnailManager::setThumbnailSize(const QSize &size)
{
    if (m_thumbnailSize == size)
        return;

    m_thumbnailSize = size;
    emit sizeHintChanged(sizeHint());
}

QImage CameraThumbnailManager::image() const
{
    if (!m_selectedCamera)
        return QImage();

    return m_thumbnailByCamera.value(m_selectedCamera).thumbnail;
}

QSize CameraThumbnailManager::sizeHint() const
{
    QSize imageSize = image().size();
    if (!imageSize.isEmpty())
        return imageSize;

    return sizeHintForCamera(m_selectedCamera, m_thumbnailSize);
}

Qn::ThumbnailStatus CameraThumbnailManager::status() const
{
    if (!m_selectedCamera)
        return Qn::ThumbnailStatus::Invalid;

    return m_thumbnailByCamera.value(m_selectedCamera).status;
}

QSize CameraThumbnailManager::sizeHintForCamera(const QnVirtualCameraResourcePtr& camera,
    const QSize& limit)
{
    // TODO: #sivanov Process camera rotation?
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
        result = Geometry::cwiseMul(stream.getResolution(), tiling);
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

Qn::ThumbnailStatus CameraThumbnailManager::statusForCamera(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera)
        return Qn::ThumbnailStatus::Invalid;

    const auto iter = m_thumbnailByCamera.constFind(camera);
    if (iter == m_thumbnailByCamera.cend())
        return Qn::ThumbnailStatus::Invalid;

    return iter->status;
}

bool CameraThumbnailManager::hasThumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera)
        return false;

    const auto iter = m_thumbnailByCamera.constFind(camera);
    if (iter == m_thumbnailByCamera.cend())
        return false;

    return iter->status == Qn::ThumbnailStatus::Loaded
        || iter->status == Qn::ThumbnailStatus::Refreshing;
}

QImage CameraThumbnailManager::thumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const
{
    if (!camera)
        return QImage();

    const auto iter = m_thumbnailByCamera.constFind(camera);
    if (iter == m_thumbnailByCamera.cend())
        return QImage();

    return iter->thumbnail;
}

rest::Handle CameraThumbnailManager::loadThumbnailForCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || !camera->hasVideo())
        return kInvalidHandle;

    nx::api::CameraImageRequest request;
    request.camera = camera;
    request.size = m_thumbnailSize;
    request.rotation = m_autoRotate
        ? nx::api::ImageRequest::kDefaultRotation
        : 0;

    auto systemContext = SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return kInvalidHandle;

    auto api = systemContext->connectedServerApi();
    if (!api)
        return kInvalidHandle;

    QPointer<CameraThumbnailManager> guard(this);
    return api->cameraThumbnailAsync(request,
        [guard, this, request](
            bool success,
            rest::Handle requestId,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& headers)
        {
            if (!guard)
                return;

            if (!m_thumbnailByCamera.contains(request.camera))
                return;

            ThumbnailData& data = m_thumbnailByCamera[request.camera];
            if (data.loadingHandle != requestId)
                return;

            Qn::ThumbnailStatus oldStatus = data.status;

            data.loadingHandle = kInvalidHandle;
            data.status = Qn::ThumbnailStatus::NoData;

            // TODO: add error message processing.
            const auto json = Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::JsonFormat);
            bool isJsonError = nx::network::http::getHeaderValue(headers, "Content-Type") == json;

            if (success && !isJsonError && !imageData.isEmpty())
            {
                const auto imageFormat =
                    QByteArray::fromStdString(nx::reflect::toString(request.format));
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

void CameraThumbnailManager::doLoadAsync()
{
    NX_ASSERT(false);
}

void CameraThumbnailManager::atCameraStatusChanged(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(camera))
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

void CameraThumbnailManager::atResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        resource->disconnect(this);
        if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
            m_thumbnailByCamera.remove(camera);

        if (m_selectedCamera == resource)
            selectCamera(QnVirtualCameraResourcePtr());
    }
}

bool CameraThumbnailManager::isUpdating(Qn::ThumbnailStatus status) const
{
    return status == Qn::ThumbnailStatus::Loading || status == Qn::ThumbnailStatus::Refreshing;
}

bool CameraThumbnailManager::isUpdateRequired(const QnVirtualCameraResourcePtr& camera,
    Qn::ThumbnailStatus status) const
{
    switch (camera->getStatus())
    {
        case nx::vms::api::ResourceStatus::recording:
            return !isUpdating(status);

        case nx::vms::api::ResourceStatus::online:
            return (status == Qn::ThumbnailStatus::NoData);

        default:
            return false;
    }
}

void CameraThumbnailManager::forceRefreshThumbnails()
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

} // namespace nx::vms::client::desktop
