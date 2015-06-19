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


QnSingleThumbnailLoader *QnSingleThumbnailLoader::newInstance(const QnVirtualCameraResourcePtr &camera,
                                                              qint64 microSecSinceEpoch,
                                                              int rotation,
                                                              const QSize &size,
                                                              ThumbnailFormat format,
                                                              QObject *parent)
{
    if (!camera)
        return NULL;

    QnMediaServerResourcePtr server;
    if (microSecSinceEpoch < 0 || microSecSinceEpoch == DATETIME_NOW) {
        server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    }
    else {
        server = QnCameraHistoryPool::instance()->getMediaServerOnTime(camera, microSecSinceEpoch / 1000, false);
    }

    if (!server)
        return NULL;

    return new QnSingleThumbnailLoader(server, camera, microSecSinceEpoch, rotation, size, format, parent);
}

QnSingleThumbnailLoader::QnSingleThumbnailLoader(const QnMediaServerResourcePtr &server,
                                                 const QnVirtualCameraResourcePtr &camera,
                                                 qint64 microSecSinceEpoch,
                                                 int rotation,
                                                 const QSize &size,
                                                 ThumbnailFormat format,
                                                 QObject *parent):
    base_type(parent),
    m_camera(camera),
    m_server(server),
    m_microSecSinceEpoch(microSecSinceEpoch),
    m_rotation(rotation),
    m_size(size),
    m_format(format)
{
    if(!server)
        qnNullWarning(server);

    if(!camera)
        qnNullWarning(camera);
}

QImage QnSingleThumbnailLoader::image() const {
    return m_image;
}

void QnSingleThumbnailLoader::doLoadAsync() {
    m_server->apiConnection()->getThumbnailAsync(
            m_camera,
            m_microSecSinceEpoch,
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

void QnSingleThumbnailLoader::at_replyReceived(int status, const QImage &image, int requstHandle) {
    Q_UNUSED(requstHandle)
    if (status == 0)
        m_image = image;
    else
        m_image = QImage();
    emit imageChanged(m_image);
}


