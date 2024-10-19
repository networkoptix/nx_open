// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <camera/thumbnail_cache_base.h>
#include <core/resource/resource_fwd.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>
#include <utils/common/id.h>

class QnCameraThumbnailCache: public QObject,
    public QnThumbnailCacheBase,
    public Singleton<QnCameraThumbnailCache>,
    public nx::vms::client::mobile::CurrentSystemContextAware
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
    QString thumbnailId(const nx::Uuid &resourceId) const;

    void refreshThumbnails(const QList<nx::Uuid> &resourceIds);
    void refreshThumbnail(const nx::Uuid &id);

signals:
    void thumbnailUpdated(const nx::Uuid &resourceId, const QString &thumbnailId);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    mutable nx::Mutex m_mutex;
    QElapsedTimer m_elapsedTimer;
    QHash<nx::Uuid, ThumbnailData> m_thumbnailByResourceId;
    QHash<QString, QPixmap> m_pixmaps;
    nx::api::CameraImageRequest m_request;
    QThread *m_decompressThread;
};
