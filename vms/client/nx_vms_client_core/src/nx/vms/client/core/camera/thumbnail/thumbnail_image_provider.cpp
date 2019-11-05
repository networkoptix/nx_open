#include "thumbnail_image_provider.h"

#include "abstract_thumbnail_provider.h"

namespace nx::vms::client::core {

const QString ThumbnailImageProvider::id = "thumbnails";

ThumbnailImageProvider::ThumbnailImageProvider():
    QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap ThumbnailImageProvider::requestPixmap(
    const QString& id, QSize* size, const QSize& /*requestedSize*/)
{
    const auto parts = id.split('/');
    if (parts.size() < 2)
        return {};

    const auto provider = m_accessorById.value(parts.first());
    if (!provider)
        return {};

    const QPixmap& pixmap = provider->pixmap(parts[1]);
    *size = pixmap.size();
    return pixmap;
}

void ThumbnailImageProvider::addProvider(
    const QString& accessorId, AbstractThumbnailProvider* provider)
{
    m_accessorById.insert(accessorId, provider);
}

void ThumbnailImageProvider::removeProvider(const QString& accessorId)
{
    m_accessorById.remove(accessorId);
}

} // namespace nx::vms::client::core
