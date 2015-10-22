#include "active_camera_thumbnail_loader_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <api/media_server_connection.h>

namespace {

enum {
    refreshInterval = 300
};

enum {
    invalidRequest = -1
};

} // anonymous namespace

QnActiveCameraThumbnailLoaderPrivate::QnActiveCameraThumbnailLoaderPrivate(
        QnActiveCameraThumbnailLoader *parent)
    : QObject(parent)
    , q_ptr(parent)
    , thumbnailSize(0, 128)
    , refreshOperation(new QnPendingOperation([this](){ refresh(); }, refreshInterval, this))
    , imageProvider(nullptr)
    , requestId(invalidRequest)
    , requestNextAfterReply(false)
{
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
                camera, position, -1, thumbnailSize,
                lit("jpg"), QnMediaServerConnection::Precise,
                this, SLOT(at_thumbnailReceived(int, const QImage&, int)));
}

void QnActiveCameraThumbnailLoaderPrivate::requestRefresh() {
    refreshOperation->requestOperation();
}

void QnActiveCameraThumbnailLoaderPrivate::at_thumbnailReceived(int status, const QImage &image, int handle) {
    Q_UNUSED(handle)

    requestId = invalidRequest;

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
