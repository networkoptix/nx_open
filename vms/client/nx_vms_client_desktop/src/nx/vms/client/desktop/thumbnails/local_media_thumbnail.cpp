// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_media_thumbnail.h"

#include <QtQml/QtQml>

#include <core/resource/avi/avi_resource.h>

#include <nx/vms/client/core/thumbnails/thumbnail_cache.h>
#include <nx/vms/client/core/thumbnails/local_media_async_image_request.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;

struct LocalMediaThumbnail::Private
{
    static ThumbnailCache* cache()
    {
        static ThumbnailCache cache;
        return &cache;
    }
};

LocalMediaThumbnail::LocalMediaThumbnail(QObject* parent):
    base_type(Private::cache(), parent),
    d(new Private)
{
}

LocalMediaThumbnail::~LocalMediaThumbnail()
{
    // Required here for forward-declared scoped pointer destruction.
}

void LocalMediaThumbnail::registerQmlType()
{
    qmlRegisterType<LocalMediaThumbnail>("nx.vms.client.desktop", 1, 0,
        "LocalMediaThumbnail");
}

std::unique_ptr<AsyncImageResult> LocalMediaThumbnail::getImageAsyncUncached() const
{
    const auto localMedia = resource().objectCast<QnAviResource>();
    if (!localMedia || !NX_ASSERT(active()))
        return {};

    return std::make_unique<LocalMediaAsyncImageRequest>(
        localMedia, LocalMediaAsyncImageRequest::kMiddlePosition, maximumSize());
}

} // namespace nx::vms::client::desktop
