#include "camera_thumbnail_provider.h"

#include "utils/common/id.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/security_cam_resource.h"
#include "camera/camera_thumbnail_cache.h"

QnCameraThumbnailProvider::QnCameraThumbnailProvider() :
    QQuickImageProvider(QQuickImageProvider::Pixmap),
    m_thumbnailCache(0)
{
}

QPixmap QnCameraThumbnailProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    if (!m_thumbnailCache)
        return QPixmap();

    return m_thumbnailCache->thumbnail(id);
}

void QnCameraThumbnailProvider::setThumbnailCache(QnCameraThumbnailCache *thumbnailCache) {
    m_thumbnailCache = thumbnailCache;
}

