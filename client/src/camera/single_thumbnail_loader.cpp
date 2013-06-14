#include "single_thumbnail_loader.h"

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>

QnSingleThumbnailLoader::QnSingleThumbnailLoader(const QnMediaServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent):
    QObject(parent),
    m_resource(resource),
    m_connection(connection)
{
    if(!connection)
        qnNullWarning(connection);

    if(!resource)
        qnNullWarning(resource);
}

QnSingleThumbnailLoader *QnSingleThumbnailLoader::newInstance(QnResourcePtr resource, QObject *parent) {
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!networkResource)
        return NULL;

    QnMediaServerResourcePtr serverResource = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(resource->getParentId()));
    if (!serverResource)
        return NULL;

    QnMediaServerConnectionPtr serverConnection = serverResource->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnSingleThumbnailLoader(serverConnection, networkResource, parent);
}

void QnSingleThumbnailLoader::load(qint64 usecSinceEpoch, const QSize& size)
{
    sendRequest(usecSinceEpoch, size);
}

int QnSingleThumbnailLoader::sendRequest(qint64 usecSinceEpoch, const QSize& size)
{
    return m_connection->getThumbnailAsync(
                m_resource.dynamicCast<QnNetworkResource>(),
                usecSinceEpoch,
                size,
                QLatin1String("png"),
                false,
                this,
                SLOT(at_replyReceived(int, const QImage&, int))
                );
}

void QnSingleThumbnailLoader::at_replyReceived(int status, const QImage &image, int requstHandle)
{
    Q_UNUSED(requstHandle)
    if (status == 0)
        emit success(image);
    else
        emit failed(status);
    emit finished();
}


