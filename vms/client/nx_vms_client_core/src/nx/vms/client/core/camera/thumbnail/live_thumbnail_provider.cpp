#include "live_thumbnail_provider.h"

#include <QtQml/QtQml>

#include <nx/utils/guarded_callback.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/server_rest_connection.h>
#include <nx/media/jpeg.h>

namespace nx::vms::client::core {

LiveThumbnailProvider::LiveThumbnailProvider(QObject* parent):
    QObject(parent),
    m_decompressionThread(new QThread(this))
{
    m_decompressionThread->setObjectName("thumbnail_decompression_" + id);
    m_decompressionThread->start();
}

LiveThumbnailProvider::~LiveThumbnailProvider()
{
    m_decompressionThread->quit();
    m_decompressionThread->wait();
}

QPixmap LiveThumbnailProvider::pixmap(const QString& thumbnailId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_pixmapById.value(thumbnailId);
}

void LiveThumbnailProvider::refresh(const QnUuid& cameraId, const QnUuid& engineId)
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);
    if (!camera)
        return;

    const QnMediaServerResourcePtr server = commonModule()->currentServer();
    if (!server)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_requestByCameraId.value(cameraId) != rest::Handle()) //< Request is running.
        return;

    const auto handleReply =
        [this, cameraId, engineId](
            bool success,
            rest::Handle requestId,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            QPixmap pixmap;
            if (success && !imageData.isEmpty())
                pixmap = QPixmap::fromImage(decompressJpegImage(imageData));

            QString thumbnailId;

            {
                NX_MUTEX_LOCKER lock(&m_mutex);

                if (requestId != m_requestByCameraId.value(cameraId))
                    return;

                m_requestByCameraId.remove(cameraId);
                if (pixmap.isNull())
                    return;

                thumbnailId = cameraId.toSimpleString() + "_" + engineId.toSimpleString();
                m_pixmapById[thumbnailId] = pixmap;
            }

            if (!thumbnailId.isEmpty())
                emit thumbnailUpdated(cameraId, makeUrl(thumbnailId));
        };

    nx::api::CameraImageRequest request;
    request.camera = camera;

    const nx::vms::api::StreamIndex analyzedStreamIndex = camera->analyzedStreamIndex(engineId);
    request.streamSelectionMode = analyzedStreamIndex == nx::vms::api::StreamIndex::primary
        ? nx::api::CameraImageRequest::StreamSelectionMode::forcedPrimary
        : nx::api::CameraImageRequest::StreamSelectionMode::forcedSecondary;

    request.size.setHeight(m_thumbnailHeight);
    request.rotation = m_rotation;

    const auto requestId = server->restConnection()->cameraThumbnailAsync(
        request, nx::utils::guarded(this, handleReply), m_decompressionThread);
    if (requestId != rest::Handle())
        m_requestByCameraId[cameraId] = requestId;
}

void LiveThumbnailProvider::refresh(const QString& cameraId, const QString& engineId)
{
    refresh(QnUuid::fromStringSafe(cameraId), QnUuid::fromStringSafe(engineId));
}

void LiveThumbnailProvider::load(const QnUuid& cameraId, const QnUuid& engineId)
{
    const auto thumbnailId = cameraId.toSimpleString() + "_" + engineId.toSimpleString();
    if (const bool loaded = !pixmap(thumbnailId).isNull())
        emit thumbnailUpdated(cameraId, makeUrl(thumbnailId));
    else
        refresh(cameraId, engineId);
}

void LiveThumbnailProvider::load(const QString& cameraId, const QString& engineId)
{
    load(QnUuid::fromStringSafe(cameraId), QnUuid::fromStringSafe(engineId));
}

void LiveThumbnailProvider::setThumbnailsHeight(int height)
{
    if (m_thumbnailHeight == height)
        return;

    m_thumbnailHeight = height;
    emit thumbnailsHeightChanged();
}

void LiveThumbnailProvider::setRotation(int rotation)
{
    if (m_rotation == rotation)
        return;

    m_rotation = rotation;
    emit rotationChanged();
}

void LiveThumbnailProvider::registerQmlType()
{
    qmlRegisterType<LiveThumbnailProvider>("nx.vms.client.core", 1, 0, "LiveThumbnailProvider");
}

} // namespace nx::vms::client::core
