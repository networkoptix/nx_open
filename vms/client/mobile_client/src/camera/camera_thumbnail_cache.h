#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <nx/api/mediaserver/image_request.h>
#include <utils/common/id.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <camera/thumbnail_cache_base.h>

class QnCameraThumbnailCache : public QObject,
    public QnThumbnailCacheBase,
    public Singleton<QnCameraThumbnailCache>,
    public QnConnectionContextAware
{
    Q_OBJECT

public:
    struct ThumbnailData
    {
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
    QString thumbnailId(const QnUuid &resourceId) const;

    void refreshThumbnails(const QList<QnUuid> &resourceIds);
    void refreshThumbnail(const QnUuid &id);

signals:
    void thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    mutable QnMutex m_mutex;
    QElapsedTimer m_elapsedTimer;
    QHash<QnUuid, ThumbnailData> m_thumbnailByResourceId;
    QHash<QString, QPixmap> m_pixmaps;
    nx::api::CameraImageRequest m_request;
    QThread *m_decompressThread;
};
