#include "layout_background_image_provider.h"

#include <core/resource/layout_resource.h>

#include <nx/client/desktop/utils/server_image_cache.h>
#include <nx/client/desktop/utils/local_file_cache.h>

#include "threaded_image_loader.h"

namespace nx {
namespace client {
namespace desktop {

struct LayoutBackgroundImageProvider::Private
{
    QScopedPointer<ServerImageCache> cache;
    QSize maxImageSize;
    QScopedPointer<ThreadedImageLoader> loader;
    QImage image;

    QSize sizeHint() const
    {
        if (!image.isNull())
            return image.size();

        if (!maxImageSize.isNull())
            return maxImageSize;

        if (!cache.isNull())
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

    d->cache.reset(layout->isFile()
        ? new LocalFileCache()
        : new ServerImageCache());
    d->maxImageSize = maxImageSize;
    d->loader.reset(new ThreadedImageLoader());
    d->loader->setInput(d->cache->getFullPath(layout->backgroundImageFilename()));
    d->loader->setSize(d->sizeHint());

    connect(d->loader, &ThreadedImageLoader::imageLoaded, this,
        [this](const QImage& image)
        {
            d->image = image;
            emit imageChanged(image);
            emit statusChanged(status());
            emit sizeHintChanged(sizeHint());
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

} // namespace desktop
} // namespace client
} // namespace nx
