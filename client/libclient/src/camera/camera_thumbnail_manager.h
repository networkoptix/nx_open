#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QImage>

#include <api/server_rest_connection_fwd.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

class QnCameraThumbnailManager : public QObject
{
    Q_OBJECT
public:
    enum ThumbnailStatus
    {
        None,
        Loading,
        Loaded,
        NoData,
        Refreshing
    };

    explicit QnCameraThumbnailManager(QObject* parent = nullptr);
    virtual ~QnCameraThumbnailManager();

    void selectResource(const QnSecurityCamResourcePtr& camera);

    QSize thumbnailSize() const;
    void setThumbnailSize(const QSize& size);

    QPixmap statusPixmap(ThumbnailStatus status);

signals:
    void statusChanged(const QnUuid& resourceId, ThumbnailStatus status);
    void thumbnailReady(const QnUuid& resourceId, const QPixmap& thumbnail);

private slots:
    void at_resPool_statusChanged(const QnResourcePtr& resource);
    void at_resPool_resourceRemoved(const QnResourcePtr& resource);

private:
    Q_SIGNAL void thumbnailReadyDelayed(const QnUuid& resourceId, const QPixmap& thumbnail);

    rest::Handle loadThumbnailForCamera(const QnSecurityCamResourcePtr& camera);

    bool isUpdateRequired(const QnSecurityCamResourcePtr& camera, const ThumbnailStatus status) const;
    void forceRefreshThumbnails();

    QPixmap scaledPixmap(const QPixmap& pixmap) const;
    void updateStatusPixmaps();

    struct ThumbnailData
    {
        ThumbnailData();

        QImage thumbnail;
        ThumbnailStatus status;
        rest::Handle loadingHandle;
    };

    QHash<QnSecurityCamResourcePtr, ThumbnailData> m_thumbnailByCamera;
    QSize m_thumnailSize;
    QHash<ThumbnailStatus, QPixmap> m_statusPixmaps;
    QTimer *m_refreshingTimer;
};
