// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_background_image_provider.h"

#include <client/client_globals.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>

#include "threaded_image_loader.h"

namespace nx::vms::client::desktop {

struct LayoutBackgroundImageProvider::Private
{
    QPointer<ServerImageCache> cache;
    QSize maxImageSize;
    QScopedPointer<ThreadedImageLoader> loader;
    QImage image;

    QSize sizeHint() const
    {
        if (!image.isNull())
            return image.size();

        if (!maxImageSize.isNull())
            return maxImageSize;

        if (cache)
            return cache->getMaxImageSize();

        return QSize();
    }
};

LayoutBackgroundImageProvider::LayoutBackgroundImageProvider(const QnLayoutResourcePtr& layout,
    const QSize& maxImageSize,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    if (layout->backgroundImageFilename().isEmpty())
        return;

    const auto systemContext = SystemContext::fromResource(layout);
    d->cache = layout->isFile()
        ? systemContext->localFileCache()
        : systemContext->serverImageCache();
    d->maxImageSize = maxImageSize;
    d->loader.reset(new ThreadedImageLoader());
    d->loader->setInput(d->cache->getFullPath(layout->backgroundImageFilename()));
    d->loader->setSize(d->sizeHint());

    connect(d->loader.get(), &ThreadedImageLoader::imageLoaded, this,
        [this](const QImage& image)
        {
            d->image = image;
            emit imageChanged(image);
            emit sizeHintChanged(sizeHint());
            emit statusChanged(status());
        });
}

LayoutBackgroundImageProvider::~LayoutBackgroundImageProvider()
{
}

QImage LayoutBackgroundImageProvider::image() const
{
    return d->image;
}

QSize LayoutBackgroundImageProvider::sizeHint() const
{
    return d->sizeHint();
}

Qn::ThumbnailStatus LayoutBackgroundImageProvider::status() const
{
    if (d->cache.isNull())
        return Qn::ThumbnailStatus::Invalid;

    return d->image.isNull()
        ? Qn::ThumbnailStatus::Loading
        : Qn::ThumbnailStatus::Loaded;
}

void LayoutBackgroundImageProvider::doLoadAsync()
{
    d->loader->start();
}

} // namespace nx::vms::client::desktop
