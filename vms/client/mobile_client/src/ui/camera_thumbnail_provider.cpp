// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_thumbnail_provider.h"

#include <nx/utils/log/assert.h>

#include "camera/thumbnail_cache_base.h"

QnCameraThumbnailProvider::QnCameraThumbnailProvider():
    QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap QnCameraThumbnailProvider::requestPixmap(
    const QString& id, QSize* size, const QSize& requestedSize)
{
    if (m_cacheById.empty())
        return {};

    const auto parts = id.split("/");
    if (!NX_ASSERT(parts.size() == 2))
        return {};

    const auto cache = m_cacheById.value(parts[0]);
    if (!cache)
        return {};

    QPixmap thumbnail = cache->getThumbnail(parts[1]);
    if (!requestedSize.isEmpty())
        thumbnail = thumbnail.scaled(requestedSize, Qt::KeepAspectRatio);

    *size = thumbnail.size();
    return thumbnail;
}

void QnCameraThumbnailProvider::addThumbnailCache(QnThumbnailCacheBase* thumbnailCache)
{
    if (NX_ASSERT(thumbnailCache))
        m_cacheById[thumbnailCache->cacheId()] = thumbnailCache;
}

void QnCameraThumbnailProvider::removeThumbnailCache(QnThumbnailCacheBase* thumbnailCache)
{
    if (NX_ASSERT(thumbnailCache))
        m_cacheById.remove(thumbnailCache->cacheId());
}
