#ifndef QNCAMERATHUMBNAILCACHE_H
#define QNCAMERATHUMBNAILCACHE_H

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>
#include <utils/common/singleton.h>
#include <camera/thumbnail_cache_base.h>

class QnCameraThumbnailCache : public QObject, public QnThumbnailCacheBase, public Singleton<QnCameraThumbnailCache> {
    Q_OBJECT
public:
    struct ThumbnailData {
        QString thumbnailId;
        qint64 time;
        bool loading;

        ThumbnailData() :
            time(0),
            loading(false)
        {}
    };

    explicit QnCameraThumbnailCache(QObject *parent = 0);
    ~QnCameraThumbnailCache();

    void start();
    void stop();

    virtual QPixmap getThumbnail(const QString &thumbnailId) const override;
    QString thumbnailId(const QnUuid &resourceId);

    void refreshThumbnails(const QList<QnUuid> &resourceIds);

signals:
    void thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_thumbnailReceived(int status, const QImage &thumbnail, int handle);

private:
    void refreshThumbnail(const QnUuid &id);

private:
    mutable QMutex m_mutex;
    QElapsedTimer m_elapsedTimer;
    QHash<QnUuid, ThumbnailData> m_thumbnailByResourceId;
    QHash<QString, QPixmap> m_pixmaps;
    QHash<int, QnUuid> m_idByRequestHandle;
    QSize m_thumbnailSize;
};

#endif // QNCAMERATHUMBNAILCACHE_H
