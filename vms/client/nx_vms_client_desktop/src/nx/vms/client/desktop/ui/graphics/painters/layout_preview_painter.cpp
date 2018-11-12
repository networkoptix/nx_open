#include "layout_preview_painter.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/common/palette.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/style/nx_style.h>
#include <ui/style/skin.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/autoscaled_plain_text.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/image_providers/layout_thumbnail_loader.h>

using nx::vms::client::core::Geometry;

namespace {
// We request this size for thumbnails.
static const QSize kPreviewSize(640, 480);
}

namespace nx::vms::client::desktop {
namespace ui {

LayoutPreviewPainter::LayoutPreviewPainter(QnResourcePool* resourcePool,
    QObject* parent)
    :
    base_type(parent),
    m_frameColor(Qt::black),
    m_backgroundColor(Qt::darkGray),
    m_itemBackgroundColor(Qt::black),
    m_fontColor(Qt::white),
    m_busyIndicator(new BusyIndicator()),
    m_resourcePool(resourcePool)
{
    NX_ASSERT(m_resourcePool);
}

LayoutPreviewPainter::~LayoutPreviewPainter()
{
}

QnLayoutResourcePtr LayoutPreviewPainter::layout() const
{
    return m_layout;
}

void LayoutPreviewPainter::setLayout(const QnLayoutResourcePtr& layout)
{
    if (m_layout)
    {
        // Disconnect from previous layout
        m_layout->disconnect(this);
        m_layoutThumbnailProvider.reset();
        m_overlayStatus = Qn::NoDataOverlay;
    }

    m_layout = layout;

    if (m_layout)
    {
        const QSize previewSize = kPreviewSize;
        m_layoutThumbnailProvider.reset(
            new LayoutThumbnailLoader(m_layout, previewSize, nx::api::ImageRequest::kLatestThumbnail));

        connect(m_layoutThumbnailProvider.get(), &ImageProvider::statusChanged,
            this, &LayoutPreviewPainter::at_updateThumbnailStatus);
        connect(m_layoutThumbnailProvider.get(), &ImageProvider::imageChanged,
            this, &LayoutPreviewPainter::at_updateThumbnailImage);

        m_layoutThumbnailProvider->setResourcePool(m_resourcePool);
        m_layoutThumbnailProvider->loadAsync();
    }
}

QColor LayoutPreviewPainter::frameColor() const
{
    return m_frameColor;
}

void LayoutPreviewPainter::setFrameColor(const QColor& value)
{
    m_frameColor = value;
}

QColor LayoutPreviewPainter::backgroundColor() const
{
    return m_backgroundColor;
}

void LayoutPreviewPainter::setBackgroundColor(const QColor& value)
{
    m_backgroundColor = value;
}

QColor LayoutPreviewPainter::itemBackgroundColor() const
{
    return m_itemBackgroundColor;
}

void LayoutPreviewPainter::setItemBackgroundColor(const QColor& value)
{
    m_itemBackgroundColor = value;
}

QColor LayoutPreviewPainter::fontColor() const
{
    return m_fontColor;
}

void LayoutPreviewPainter::setFontColor(const QColor& value)
{
    m_fontColor = value;
}

void LayoutPreviewPainter::paint(QPainter* painter, const QRect& paintRect)
{
    static const int kFrameWidth = 1;

    if (paintRect.isEmpty())
        return;

    if (!m_layout)
    {
        QnNxStyle::paintCosmeticFrame(painter, paintRect, m_frameColor, kFrameWidth, 0);
        return;
    }
    painter->fillRect(paintRect, m_backgroundColor);

    Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;
    if (m_layoutThumbnailProvider)
        status = m_layoutThumbnailProvider->status();

    if (m_layout && !m_layoutThumbnail.isNull() && (status == Qn::ThumbnailStatus::Loading || status == Qn::ThumbnailStatus::Loaded))
    {
        QSizeF paintSize = m_layoutThumbnail.size() / m_layoutThumbnail.devicePixelRatio();
        // Fitting thumbnail exactly to widget's rect.
        paintSize = Geometry::bounded(paintSize, paintRect.size(), Qt::KeepAspectRatio);
        paintSize = Geometry::expanded(paintSize, paintRect.size(), Qt::KeepAspectRatio);

        QRect dstRect = QStyle::alignedRect(painter->layoutDirection(), Qt::AlignCenter,
            paintSize.toSize(), paintRect);

        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter->drawPixmap(dstRect, m_layoutThumbnail);
    }

    QnNxStyle::paintCosmeticFrame(painter, paintRect, m_frameColor, kFrameWidth, 0);
}

void LayoutPreviewPainter::at_updateThumbnailStatus(Qn::ThumbnailStatus status)
{
    switch (status)
    {
    case Qn::ThumbnailStatus::Loaded:
        m_overlayStatus = Qn::EmptyOverlay;
        break;

    case Qn::ThumbnailStatus::Loading:
        m_overlayStatus = Qn::LoadingOverlay;
        break;

    default:
        m_overlayStatus = Qn::NoDataOverlay;
        break;
    }

}

void LayoutPreviewPainter::at_updateThumbnailImage(const QImage& image)
{
    m_layoutThumbnail = QPixmap::fromImage(image);
    // TODO: ping parent that data is updated
}

} // namespace ui
} // namespace nx::vms::client::desktop

