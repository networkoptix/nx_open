#include "resource_widget_pixmap_cache.h"

namespace nx::vms::client::desktop {

ResourceWidgetPixmapCache::ResourceWidgetPixmapCache(QObject* parent):
    QObject(parent)
{
}

QPixmap ResourceWidgetPixmapCache::pixmap(const QString& fileName) const
{
    QPixmap pixmap;
    if (!m_pixmapCache.find(fileName, &pixmap))
    {
        if (pixmap.load(fileName))
            m_pixmapCache.insert(fileName, pixmap);
    }
    return pixmap;
}

} // namespace nx::vms::client::desktop
