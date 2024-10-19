// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>
#include <QtCore/QThread>

#include <core/resource/resource_fwd.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <ui/camera_thumbnail_provider.h>

#include "../active_camera_thumbnail_loader.h"

class QnActiveCameraThumbnailLoaderPrivate:
    public QObject,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QnActiveCameraThumbnailLoader)

    QnActiveCameraThumbnailLoader* q_ptr;

public:
    QnActiveCameraThumbnailLoaderPrivate(QnActiveCameraThumbnailLoader* parent);
    ~QnActiveCameraThumbnailLoaderPrivate();

    mutable QMutex thumbnailMutex;

    int currentQuality = 0;
    QVector<int> screenshotQualityList;

    QnVirtualCameraResourcePtr camera;
    qint64 position;
    QString thumbnailId;

    QPixmap thumbnailPixmap;

    nx::utils::PendingOperation* refreshOperation = nullptr;

    QnCameraThumbnailProvider* imageProvider = nullptr;

    nx::api::CameraImageRequest request;
    int requestId;
    bool requestNextAfterReply = false;
    QElapsedTimer fetchTimer;

    QThread* decompressThread;

    QPixmap thumbnail() const;
    void clear();
    void refresh(bool force = false);
    void requestRefresh();

    QSize currentSize() const;
};
