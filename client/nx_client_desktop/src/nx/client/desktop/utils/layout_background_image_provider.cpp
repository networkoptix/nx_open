#include "layout_background_image_provider.h"

#include <core/resource/layout_resource.h>

#include "server_image_cache.h"
#include "local_file_cache.h"
#include <utils/threaded_image_loader.h>

namespace nx {
namespace client {
namespace desktop {

struct LayoutBackgroundImageProvider::Private
{
    QScopedPointer<ServerImageCache> cache;
    QSize maxImageSize;
    QScopedPointer<QnThreadedImageLoader> loader;
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
    d->loader.reset(new QnThreadedImageLoader());
    d->loader->setInput(d->cache->getFullPath(layout->backgroundImageFilename()));
    d->loader->setSize(d->sizeHint());

#define QnThreadedImageLoaderFinished \
        static_cast<void (QnThreadedImageLoader::*)(const QImage& )> \
        (&QnThreadedImageLoader::finished)

    connect(d->loader, QnThreadedImageLoaderFinished, this,
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
