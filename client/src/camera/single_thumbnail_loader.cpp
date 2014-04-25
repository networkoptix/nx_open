#include "single_thumbnail_loader.h"

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>


QnSingleThumbnailLoader *QnSingleThumbnailLoader::newInstance(QnResourcePtr resource,
                                                              qint64 microSecSinceEpoch,
                                                              const QSize &size,
                                                              ThumbnailFormat format,
                                                              QObject *parent)
{
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource)
        return NULL;

    QnMediaServerResourcePtr serverResource = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(resource->getParentId()));
    if (!serverResource)
        return NULL;

    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnSingleThumbnailLoader(serverConnection, networkResource, microSecSinceEpoch, size, format, parent);
}

QnSingleThumbnailLoader::QnSingleThumbnailLoader(const QnMediaServerConnectionPtr &connection,
                                                 QnNetworkResourcePtr resource,
                                                 qint64 microSecSinceEpoch,
                                                 const QSize &size,
                                                 ThumbnailFormat format,
                                                 QObject *parent):
    base_type(parent),
    m_resource(resource),
    m_connection(connection),
    m_microSecSinceEpoch(microSecSinceEpoch),
    m_size(size),
    m_format(format)
{
    if(!connection)
        qnNullWarning(connection);

    if(!resource)
        qnNullWarning(resource);
}

QImage QnSingleThumbnailLoader::image() const {
    return m_image;
}

void QnSingleThumbnailLoader::doLoadAsync() {
    m_connection->getThumbnailAsync(
            m_resource.dynamicCast<QnNetworkResource>(),
            m_microSecSinceEpoch,
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


