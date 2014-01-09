#include "single_thumbnail_loader.h"

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>


QnSingleThumbnailLoader *QnSingleThumbnailLoader::newInstance(QnResourcePtr resource,
                                                              qint64 microSecSinceEpoch,
                                                              const QSize &size,
                                                              QObject *parent) {
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource)
        return NULL;

    QnMediaServerResourcePtr serverResource = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(resource->getParentId()));
    if (!serverResource)
        return NULL;

    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnSingleThumbnailLoader(serverConnection, networkResource, microSecSinceEpoch, size, parent);
}

QnSingleThumbnailLoader::QnSingleThumbnailLoader(const QnMediaServerConnectionPtr &connection,
                                                 QnNetworkResourcePtr resource,
                                                 qint64 microSecSinceEpoch,
                                                 const QSize &size,
                                                 QObject *parent):
    base_type(parent),
    m_resource(resource),
    m_connection(connection),
    m_microSecSinceEpoch(microSecSinceEpoch),
    m_size(size)
{
    if(!connection)
        qnNullWarning(connection);

    if(!resource)
        qnNullWarning(resource);

    connect(this, SIGNAL(success(QImage)), this, SIGNAL(imageChanged(QImage)));
}

QImage QnSingleThumbnailLoader::image() const {
    return m_image;
}

void QnSingleThumbnailLoader::doLoadAsync()
{
    m_connection->getThumbnailAsync(
            m_resource.dynamicCast<QnNetworkResource>(),
            m_microSecSinceEpoch,
            m_size,
            QLatin1String("png"),
            QnMediaServerConnection::Precise,
            this,
            SLOT(at_replyReceived(int, const QImage&, int)));
}

void QnSingleThumbnailLoader::at_replyReceived(int status, const QImage &image, int requstHandle)
{
    Q_UNUSED(requstHandle)
    if (status == 0) {
        m_image = image;
        emit success(image);
    }
    else
        emit failed(status);
    emit finished();
}


