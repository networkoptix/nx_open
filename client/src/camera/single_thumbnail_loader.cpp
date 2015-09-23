#include "single_thumbnail_loader.h"

#include <api/media_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

QnSingleThumbnailLoader::QnSingleThumbnailLoader(const QnVirtualCameraResourcePtr &camera,
                                                 const QnMediaServerResourcePtr &server,
                                                 qint64 msecSinceEpoch,
                                                 int rotation,
                                                 const QSize &size,
                                                 ThumbnailFormat format,
                                                 QObject *parent):
    base_type(parent),
    m_camera(camera),
    m_server(server),
    m_image(),
    m_msecSinceEpoch(msecSinceEpoch),
    m_rotation(rotation),
    m_size(size),
    m_format(format)
{
    Q_ASSERT_X(camera, Q_FUNC_INFO, "Camera must exist here");

    if (!m_server && camera)
        m_server = camera->getParentServer();
}

QImage QnSingleThumbnailLoader::image() const {
    return m_image;
}

void QnSingleThumbnailLoader::doLoadAsync() {
    if (    !m_server 
        ||  !m_server->apiConnection() 
        ||  !m_camera
        )
        return;

    m_server->apiConnection()->getThumbnailAsync(
            m_camera,
            m_msecSinceEpoch * 1000,
            m_rotation,
            m_size,
            formatToString(m_format),
            QnMediaServerConnection::Precise,
            this,
            SLOT(at_replyReceived(int, const QImage&, int)));
}

QString QnSingleThumbnailLoader::formatToString(ThumbnailFormat format) {
    switch (format) {
    case PngFormat:
        return lit("png");
    case JpgFormat:
        return lit("jpg");
    default:
        break;
    }
    return QString();
}

void QnSingleThumbnailLoader::at_replyReceived(int status, const QImage &image, int requestHandle) {
    Q_UNUSED(requestHandle)
    if (status == 0)
        m_image = image;
    else
        m_image = QImage();
    emit imageChanged(m_image);
}


