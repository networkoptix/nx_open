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
    
signals:
    void thumbnailReady(int resourceId, const QPixmap& thumbnail);

private slots:
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_thumbnailReceived(int status, const QImage& thumbnail, int handle);
private:
    Q_SIGNAL void thumbnailReadyDelayed(int resourceId, const QPixmap& thumbnail);
    int loadThumbnailForResource(const QnResourcePtr &resource);

    enum ThumbnailStatus {
        None,
        Loading,
        Loaded,
        NoData,
        NoSignal
    };

    struct ThumbnailData {
        ThumbnailData(): status(None), loadingHandle(0) {}

        QImage thumbnail;
        ThumbnailStatus status;
        int loadingHandle;
    };

    QHash<QnResourcePtr, ThumbnailData> m_thumbnailByResource;

};

#endif // CAMERA_THUMBNAIL_MANAGER_H
