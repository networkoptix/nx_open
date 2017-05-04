#pragma once

#include <QtCore/QThread>
#include <QtCore/QElapsedTimer>

#include "../active_camera_thumbnail_loader.h"

#include <core/resource/resource_fwd.h>
#include <utils/common/pending_operation.h>
#include <ui/camera_thumbnail_provider.h>
#include <api/helpers/thumbnail_request_data.h>

class QnActiveCameraThumbnailLoaderPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(QnActiveCameraThumbnailLoader)

    QnActiveCameraThumbnailLoader *q_ptr;
public:
    QnActiveCameraThumbnailLoaderPrivate(QnActiveCameraThumbnailLoader *parent);
    ~QnActiveCameraThumbnailLoaderPrivate();

    mutable QMutex thumbnailMutex;

    int currentQuality;
    QVector<int> screenshotQualityList;

    QnVirtualCameraResourcePtr camera;
    qint64 position;
    QString thumbnailId;

    QPixmap thumbnailPixmap;

    QnPendingOperation *refreshOperation;

    QnCameraThumbnailProvider *imageProvider;

    QnThumbnailRequestData request;
    int requestId;
    bool requestNextAfterReply;
    QElapsedTimer fetchTimer;

    QThread *decompressThread;

    QPixmap thumbnail() const;
    void clear();
    void refresh(bool force = false);
    void requestRefresh();

    QSize currentSize() const;
};
