#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QImage>

#include <api/server_rest_connection_fwd.h>

#include <client_core/connection_context_aware.h>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

#include "image_provider.h"

/**
 * Keeps a cache of thumbnails
 */
class QnCameraThumbnailCache: public QObject, public QnConnectionContextAware
{
    Q_OBJECT

public:
    explicit QnCameraThumbnailCache(QObject* parent = nullptr);
    virtual ~QnCameraThumbnailCache();

    bool autoRefresh() const;
    void setAutoRefresh(bool value);

    static QSize sizeHintForCamera(const QnVirtualCameraResourcePtr& camera, const QSize& limit);

    Qn::ThumbnailStatus statusForCamera(const QnVirtualCameraResourcePtr& camera) const;
    bool hasThumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const;
    QImage thumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const;

signals:
    void thumbnailReady(const QnUuid& resourceId, const QPixmap& thumbnail);

private:
    void at_resPool_statusChanged(const QnResourcePtr& resource);
    void at_resPool_resourceRemoved(const QnResourcePtr& resource);

private:
    void refreshSelectedCamera(QnVirtualCameraResourcePtr m_selectedCamera);
    rest::Handle loadThumbnailForCamera(const QnVirtualCameraResourcePtr& camera, QSize m_thumbnailSize);

    bool isRequestAvailable(const QnVirtualCameraResourcePtr& camera) const;

    bool isUpdating(Qn::ThumbnailStatus status) const;

    bool isUpdateRequired(const QnVirtualCameraResourcePtr& camera,
        Qn::ThumbnailStatus status) const;

    void forceRefreshThumbnails();

    struct CacheItem
    {
        QSharedPointer<nx::client::desktop::ImageProvider> provider;
        Qn::ThumbnailStatus status;
        quint64 timestamp;
    };

    QHash<QnVirtualCameraResourcePtr, CacheItem> m_thumbnailByCamera;
    QTimer* m_refreshingTimer;
    bool m_autoRotate = true;
};
