#pragma once

#include "../active_camera_thumbnail_loader.h"

#include <core/resource/resource_fwd.h>
#include <utils/common/pending_operation.h>
#include <ui/camera_thumbnail_provider.h>

class QnActiveCameraThumbnailLoaderPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(QnActiveCameraThumbnailLoader)

    QnActiveCameraThumbnailLoader *q_ptr;
public:
    QnActiveCameraThumbnailLoaderPrivate(QnActiveCameraThumbnailLoader *parent);

    mutable QMutex thumbnailMutex;

    int currentQuality;
    QVector<int> screenshotQualityList;

    QnVirtualCameraResourcePtr camera;
    qint64 position;
    QString thumbnailId;

    QPixmap thumbnailPixmap;

    QnPendingOperation *refreshOperation;

    QnCameraThumbnailProvider *imageProvider;

    int requestId;
    bool requestNextAfterReply;
    QElapsedTimer fetchTimer;

    QPixmap thumbnail() const;
    void clear();
    void refresh();
    void requestRefresh();

    QSize currentSize() const;

private slots:
    void at_thumbnailReceived(int status, const QImage &image, int handle);
};
