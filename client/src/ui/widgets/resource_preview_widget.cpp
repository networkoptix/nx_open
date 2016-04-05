#include "resource_preview_widget.h"

#include <camera/camera_thumbnail_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <ui/common/geometry.h>

QnResourcePreviewWidget::QnResourcePreviewWidget(QWidget* parent /*= nullptr*/) :
    base_type(parent),
    m_thumbnailManager(new QnCameraThumbnailManager()),
    m_target(),
    m_pixmap()
{
    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this, [this](const QnUuid& resourceId, const QPixmap& thumbnail)
    {
        if (m_target != resourceId)
            return;

        m_pixmap = thumbnail;
        updateGeometry();
        update();
    });

}

QnResourcePreviewWidget::~QnResourcePreviewWidget()
{

}

QnUuid QnResourcePreviewWidget::targetResource() const
{
    return m_target;
}

void QnResourcePreviewWidget::setTargetResource(const QnUuid &target)
{
    if (m_target == target)
        return;
    m_target = target;

    QnResourcePtr targetResource = qnResPool->getResourceById(m_target);
    if (!targetResource)
        return;

    if (QnVirtualCameraResourcePtr camera = targetResource.dynamicCast<QnVirtualCameraResource>())
        m_thumbnailManager->selectResource(camera);
}

QSize QnResourcePreviewWidget::thumbnailSize() const
{
    return m_thumbnailManager->thumbnailSize();
}

void QnResourcePreviewWidget::setThumbnailSize(const QSize &size)
{
    m_thumbnailManager->setThumbnailSize(size);
}

void QnResourcePreviewWidget::paintEvent(QPaintEvent *event)
{
    base_type::paintEvent(event);
    if (m_pixmap.isNull())
        return;

    QScopedPointer<QPainter> painter(new QPainter(this));
    QRect paintRect = this->rect();
    if (paintRect.isNull())
        return;

    painter->fillRect(paintRect, palette().window());
    painter->drawPixmap(pixmapRect(), m_pixmap);
}

QSize QnResourcePreviewWidget::sizeHint() const
{
    if (m_pixmap.isNull())
        return QSize();
    return pixmapRect().size();
}

QSize QnResourcePreviewWidget::minimumSizeHint() const
{
    return pixmapRect().size();
}

QRect QnResourcePreviewWidget::pixmapRect() const
{
    QRect baseRect = this->rect();
    if (m_pixmap.isNull())
        return baseRect;
    if (m_pixmap.width() == baseRect.width())
        return m_pixmap.rect();

    QRect maxRect(baseRect);
    maxRect.setHeight(m_pixmap.height());

    return QnGeometry::expanded(QnGeometry::aspectRatio(m_pixmap.size()), maxRect, Qt::KeepAspectRatio, Qt::AlignHCenter).toRect();
}
