#ifndef CAMERA_THUMBNAIL_MANAGER_H
#define CAMERA_THUMBNAIL_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QImage>

#include <core/resource/resource_fwd.h>

class QnCameraThumbnailManager : public QObject
{
    Q_OBJECT
public:
    explicit QnCameraThumbnailManager(QObject *parent = 0);
    virtual ~QnCameraThumbnailManager();
    
    void selectResource(const QnResourcePtr &resource);
    void setThumbnailSize(const QSize &size);
signals:
    void thumbnailReady(int resourceId, const QPixmap& thumbnail);

private slots:
    void at_resPool_statusChanged(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_thumbnailReceived(int status, const QImage& thumbnail, int handle);

private:
    enum ThumbnailStatus {
        None,
        Loading,
        Loaded,
        NoData,
        NoSignal,
        Refreshing
    };

    Q_SIGNAL void thumbnailReadyDelayed(int resourceId, const QPixmap& thumbnail);
    int loadThumbnailForResource(const QnResourcePtr &resource);

    bool isUpdateRequired(const QnResourcePtr &resource, const ThumbnailStatus status) const;
    void forceRefreshThumbnails();

    struct ThumbnailData {
        ThumbnailData(): status(None), loadingHandle(0) {}

        QImage thumbnail;
        ThumbnailStatus status;
        int loadingHandle;
    };

    QHash<QnResourcePtr, ThumbnailData> m_thumbnailByResource;
    QSize m_thumnailSize;
    QHash<ThumbnailStatus, QPixmap> m_statusPixmaps;
    QTimer *m_refreshingTimer;
};

#endif // CAMERA_THUMBNAIL_MANAGER_H
