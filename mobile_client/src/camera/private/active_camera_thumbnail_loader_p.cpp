#include "active_camera_thumbnail_loader_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <api/media_server_connection.h>

namespace {

enum {
    refreshInterval = 300
    , fastTimeout = 100
    , longTimeout = 230
};

enum {
    invalidRequest = -1
};

} // anonymous namespace

QnActiveCameraThumbnailLoaderPrivate::QnActiveCameraThumbnailLoaderPrivate(
        QnActiveCameraThumbnailLoader *parent)
    : QObject(parent)
    , q_ptr(parent)
    , currentQuality(0)
    , refreshOperation(new QnPendingOperation([this](){ refresh(); }, refreshInterval, this))
    , imageProvider(nullptr)
    , requestId(invalidRequest)
    , requestNextAfterReply(false)
{
    screenshotQualityList.append(128);
    screenshotQualityList.append(240);
    screenshotQualityList.append(320);
    screenshotQualityList.append(480);
    screenshotQualityList.append(560);
    screenshotQualityList.append(720);
    screenshotQualityList.append(1080);
}

QPixmap QnActiveCameraThumbnailLoaderPrivate::thumbnail() const {
    QMutexLocker lk(&thumbnailMutex);
    return thumbnailPixmap;
}

void QnActiveCameraThumbnailLoaderPrivate::clear() {
    QMutexLocker lk(&thumbnailMutex);
    thumbnailPixmap = QPixmap();
    thumbnailId = QString();
}

void QnActiveCameraThumbnailLoaderPrivate::refresh() {
    if (requestId != invalidRequest) {
        requestNextAfterReply = true;
        return;
    }

    if (!camera)
        return;

    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    requestId = server->apiConnection()->getThumbnailAsync(
                camera, position, -1, currentSize(),
                lit("jpg"), QnMediaServerConnection::Precise,
                this, SLOT(at_thumbnailReceived(int, const QImage&, int)));
    fetchTimer.start();
}

void QnActiveCameraThumbnailLoaderPrivate::requestRefresh() {
    refreshOperation->requestOperation();
}

QSize QnActiveCameraThumbnailLoaderPrivate::currentSize() const {
    return QSize(0, screenshotQualityList[currentQuality]);
}

void QnActiveCameraThumbnailLoaderPrivate::at_thumbnailReceived(int status, const QImage &image, int handle) {
    Q_UNUSED(handle)

    requestId = invalidRequest;

    if (currentQuality < screenshotQualityList.size() - 1 && !fetchTimer.hasExpired(fastTimeout))
        ++currentQuality;
    else if (currentQuality > 0 && fetchTimer.hasExpired(longTimeout))
        --currentQuality;

    if (requestNextAfterReply) {
        requestNextAfterReply = false;
        refresh();
    }

    if (status != 0)
        return;

    thumbnailPixmap = QPixmap::fromImage(image);

    Q_Q(QnActiveCameraThumbnailLoader);
    if (camera && position > 0) {
        thumbnailId = lit("%1/%2").arg(camera->getId().toString()).arg(position);
        q->thumbnailIdChanged();
    }
}
