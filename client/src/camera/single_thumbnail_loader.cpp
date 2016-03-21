#include "single_thumbnail_loader.h"

#include <api/server_rest_connection.h>

#include <camera/camera_thumbnail_manager.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <utils/common/model_functions.h>

QnSingleThumbnailLoader::QnSingleThumbnailLoader(const QnVirtualCameraResourcePtr &camera,
                                                 qint64 msecSinceEpoch,
                                                 int rotation,
                                                 const QSize &size,
                                                 QnThumbnailRequestData::ThumbnailFormat format,
                                                 QSharedPointer<QnCameraThumbnailManager> statusPixmapManager,
                                                 QObject *parent)
    : base_type(parent)
    , m_request()
{
    Q_ASSERT_X(camera, Q_FUNC_INFO, "Camera must exist here");
    Q_ASSERT_X(qnCommon->currentServer(), Q_FUNC_INFO, "We must be connected here");

    m_request.camera = camera;
    m_request.msecSinceEpoch = msecSinceEpoch;
    m_request.rotation = rotation;
    m_request.size = size;
    m_request.imageFormat = format;

    if (!camera || !camera->hasVideo(nullptr))
    {
        if (statusPixmapManager)
        {
            QPixmap statusPixmap = statusPixmapManager->statusPixmap(QnCameraThumbnailManager::NoData);
            m_image = statusPixmap.toImage();
        }
        return;
    }

    if (statusPixmapManager)
    {
        QPixmap statusPixmap = statusPixmapManager->statusPixmap(QnCameraThumbnailManager::Loading);
        m_image = statusPixmap.toImage();
    }

    /* Making connection through event loop to handle data from another thread. */
    connect(this, &QnSingleThumbnailLoader::imageLoaded, this, [this, size, format, statusPixmapManager](const QByteArray &data)
    {
        if (!data.isEmpty())
        {
            QByteArray imageFormat = QnLexical::serialized<QnThumbnailRequestData::ThumbnailFormat>(format).toUtf8();
            m_image.loadFromData(data, imageFormat);
        }
        else if (statusPixmapManager)
        {
            QPixmap statusPixmap = statusPixmapManager->statusPixmap(QnCameraThumbnailManager::NoData);
            m_image = statusPixmap.toImage();
        }

        emit imageChanged(m_image);
    }
    , Qt::QueuedConnection);

}

QImage QnSingleThumbnailLoader::image() const {
    return m_image;
}

void QnSingleThumbnailLoader::doLoadAsync() {
    if (!qnCommon->currentServer())
    {
        emit imageLoaded(QByteArray());
        return;
    }

    QPointer<QnSingleThumbnailLoader> guard(this);
    auto handle = qnCommon->currentServer()->restConnection()->cameraThumbnailAsync(m_request,  [this, guard] (bool success, rest::Handle id, const QByteArray &imageData)
    {
        if (!guard)
            return;

        Q_UNUSED(id);
        if (!success)
            return;
        emit imageLoaded(imageData);
    });

    if (handle <= 0)
    {
        emit imageLoaded(QByteArray());
    }
}



