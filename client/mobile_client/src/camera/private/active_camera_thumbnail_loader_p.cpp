#include "active_camera_thumbnail_loader_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <nx/media/jpeg.h>

namespace {

constexpr int refreshInterval = 300;
constexpr int fastTimeout = 100;
constexpr int longTimeout = 230;
constexpr int invalidRequest = -1;

} // namespace

QnActiveCameraThumbnailLoaderPrivate::QnActiveCameraThumbnailLoaderPrivate(
    QnActiveCameraThumbnailLoader* parent)
    :
    QObject(parent),
    q_ptr(parent),
    refreshOperation(
        new nx::utils::PendingOperation(
            [this]{ refresh(); },
            refreshInterval,
            this)),
    requestId(invalidRequest),
    decompressThread(new QThread(this))
{
    screenshotQualityList.append(128);
    screenshotQualityList.append(240);
    screenshotQualityList.append(320);
    screenshotQualityList.append(480);
    screenshotQualityList.append(560);
    screenshotQualityList.append(720);
    screenshotQualityList.append(1080);

    decompressThread->setObjectName(lit("QnActiveCameraThumbnailLoader_decompressThread"));
    decompressThread->start();
}

QnActiveCameraThumbnailLoaderPrivate::~QnActiveCameraThumbnailLoaderPrivate()
{
    decompressThread->quit();
    decompressThread->wait();
}

QPixmap QnActiveCameraThumbnailLoaderPrivate::thumbnail() const
{
    QMutexLocker lk(&thumbnailMutex);
    return thumbnailPixmap;
}

void QnActiveCameraThumbnailLoaderPrivate::clear()
{
    QMutexLocker lk(&thumbnailMutex);
    thumbnailPixmap = QPixmap();
    thumbnailId = QString();
}

void QnActiveCameraThumbnailLoaderPrivate::refresh(bool force)
{
    if (force)
        requestId = invalidRequest;

    if (requestId != invalidRequest)
    {
        requestNextAfterReply = true;
        return;
    }

    if (!camera)
        return;

    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    request.camera = camera;
    request.msecSinceEpoch = position;
    request.size = currentSize();

    auto handleReply = [this] (bool success, rest::Handle handleId, const QByteArray &imageData)
    {
        if (requestId != handleId)
            return;

        requestId = invalidRequest;

        if (currentQuality < screenshotQualityList.size() - 1 && !fetchTimer.hasExpired(fastTimeout))
            ++currentQuality;
        else if (currentQuality > 0 && fetchTimer.hasExpired(longTimeout))
            --currentQuality;

        if (requestNextAfterReply)
        {
            requestNextAfterReply = false;
            refresh();
        }

        if (!success || imageData.isEmpty())
            return;

        {
            QMutexLocker lk(&thumbnailMutex);
            thumbnailPixmap = QPixmap::fromImage(decompressJpegImage(imageData));
        }

        Q_Q(QnActiveCameraThumbnailLoader);
        if (camera)
        {
            thumbnailId = lit("%1/%2").arg(camera->getId().toString()).arg(position);
            emit q->thumbnailIdChanged();
        }
    };

    requestId = server->restConnection()->cameraThumbnailAsync(request, handleReply, decompressThread);
    fetchTimer.start();
}

void QnActiveCameraThumbnailLoaderPrivate::requestRefresh()
{
    refreshOperation->requestOperation();
}

QSize QnActiveCameraThumbnailLoaderPrivate::currentSize() const
{
    return QSize(0, screenshotQualityList[currentQuality]);
}
