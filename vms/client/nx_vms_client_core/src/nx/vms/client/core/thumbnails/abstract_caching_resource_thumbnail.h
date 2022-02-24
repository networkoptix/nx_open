// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include "abstract_resource_thumbnail.h"

namespace nx::vms::client::core {

class ThumbnailCache;

/**
 * A base class of QML-ready resource thumbnails stored in a common cache.
 * Thus multiple instances of AbstractCachingResourceThumbnail represent several access points
 * to a single shared thumbnail image.
 */
class AbstractCachingResourceThumbnail: public AbstractResourceThumbnail
{
    Q_OBJECT
    using base_type = AbstractResourceThumbnail;

public:
    explicit AbstractCachingResourceThumbnail(ThumbnailCache* cache, QObject* parent = nullptr);
    virtual ~AbstractCachingResourceThumbnail() override = default;

protected:
    /**
     * Thumbnail cache key.
     * Usually an unique mapping of thumbnail source parameters: resource, timestamp, size etc.
     * Thumbnails with the same `thumbnailId` share the same image / cache entry.
     */
    virtual QString thumbnailId() const;

    /** Asynchronous image loading mechanism. */
    virtual std::unique_ptr<AsyncImageResult> getImageAsyncUncached() const = 0;

    /**
     * Cache-aware image loading mechanism.
     * Can be reimplemented in descendants for different caching policies.
     */
    virtual std::unique_ptr<AsyncImageResult> getImageAsync(bool forceRefresh) const override;

private:
    QPointer<ThumbnailCache> m_cache;
};

} // namespace nx::vms::client::core
