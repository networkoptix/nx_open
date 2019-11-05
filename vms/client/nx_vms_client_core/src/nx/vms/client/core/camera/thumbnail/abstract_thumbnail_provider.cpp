#include "abstract_thumbnail_provider.h"

#include "thumbnail_image_provider.h"

namespace nx::vms::client::core {

AbstractThumbnailProvider::AbstractThumbnailProvider(const QString& id):
    id(id)
{
    ThumbnailImageProvider::instance()->addProvider(id, this);
}

AbstractThumbnailProvider::~AbstractThumbnailProvider()
{
    ThumbnailImageProvider::instance()->removeProvider(id);
}

QUrl AbstractThumbnailProvider::makeUrl(const QString& thumbnailId) const
{
    return QStringLiteral("image://%1/%2/%3").arg(ThumbnailImageProvider::id, id, thumbnailId);
}

} // namespace nx::vms::client::core
