// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_caching_resource_thumbnail.h"

#include <core/resource/resource.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

#include "immediate_image_result.h"
#include "thumbnail_cache.h"

namespace nx::vms::client::core {

AbstractCachingResourceThumbnail::AbstractCachingResourceThumbnail(
    ThumbnailCache* cache,
    QObject* parent)
    :
    m_cache(cache)
{
    if (!NX_ASSERT(cache))
        return;

    connect(cache, &ThumbnailCache::updated, this,
        [this](const QString& key, const QImage& image)
        {
            if (key != thumbnailId())
                return;

            NX_VERBOSE(this, "Handling cache entry update (%1) (%2)", key, image.size());
            setImage(image);
        });

    connect(this, &AbstractResourceThumbnail::updated, this,
        [this]()
        {
            if (!m_cache)
                return;

            NX_VERBOSE(this, "Updating cache entry (%1) (%2)", thumbnailId(), image().size());
            m_cache->insert(thumbnailId(), image());
        });
}

std::unique_ptr<AsyncImageResult> AbstractCachingResourceThumbnail::getImageAsync(
    bool forceRefresh) const
{
    const auto cachedImage = m_cache && !forceRefresh
        ? m_cache->image(thumbnailId())
        : nullptr;

    if (cachedImage)
    {
        NX_VERBOSE(this, "Fetching image from cache (%1) (%2)", thumbnailId(), cachedImage->size());
        return std::make_unique<ImmediateImageResult>(*cachedImage);
    }

    NX_VERBOSE(this, "Requesting new image (%1)", thumbnailId());
    return getImageAsyncUncached();
}

QString AbstractCachingResourceThumbnail::thumbnailId() const
{
    const auto resourceId = (resource() ? resource()->getId() : QnUuid()).toSimpleString();
    if (maximumSize() <= 0)
        return nx::format("%1:%2").args(resourceId, timestampMs());

    return nx::format("%1:%2:%3").args(resourceId, maximumSize(), timestampMs());
}

} // namespace nx::vms::client::core
