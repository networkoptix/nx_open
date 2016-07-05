#include "camera_thumbnail_provider.h"

#include "camera/thumbnail_cache_base.h"

QnCameraThumbnailProvider::QnCameraThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_thumbnailCache(nullptr)
{
}

QPixmap QnCameraThumbnailProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    if (!m_thumbnailCache)
        return QPixmap();

    QPixmap thumbnail = m_thumbnailCache->getThumbnail(id);
    if (requestedSize.isEmpty()) {
        *size = thumbnail.size();
    } else {
        thumbnail = thumbnail.scaled(requestedSize, Qt::KeepAspectRatio);
        *size = thumbnail.size();
    }

    return thumbnail;
}

void QnCameraThumbnailProvider::setThumbnailCache(QnThumbnailCacheBase *thumbnailCache) {
    m_thumbnailCache = thumbnailCache;
}

