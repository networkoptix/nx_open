// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <camera/thumbnail_cache_base.h>
#include <core/resource/resource_fwd.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/mobile/system_context_aware.h>
#include <utils/common/id.h>

class QImage;

class QnCameraThumbnailCache:
    public QnThumbnailCacheBase,
    public nx::vms::client::mobile::SystemContextAware
{
    Q_OBJECT

public:
    explicit QnCameraThumbnailCache(nx::vms::client::mobile::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~QnCameraThumbnailCache() override;

    virtual QString cacheId() const override;
    virtual QPixmap getThumbnail(const QString& thumbnailId) const override;
    QString thumbnailId(const nx::Uuid& resourceId) const;

    void refreshThumbnails(const QList<nx::Uuid>& resourceIds);
    void refreshThumbnail(const nx::Uuid& id);

signals:
    void thumbnailUpdated(const nx::Uuid& resourceId, const QString& thumbnailId);

private:
    void start();
    void stop();

    void handleImageLoaded(const nx::Uuid& id, const QImage& image);

    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

private:
    struct ThumbnailData;
    mutable nx::Mutex m_mutex;
    QElapsedTimer m_elapsedTimer;
    QHash<nx::Uuid, ThumbnailData> m_thumbnailByResourceId;
    QHash<QString, QPixmap> m_pixmaps;
};
