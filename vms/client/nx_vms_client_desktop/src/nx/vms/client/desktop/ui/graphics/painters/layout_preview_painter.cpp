// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_preview_painter.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/autoscaled_plain_text.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/image_providers/layout_thumbnail_loader.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <ui/common/palette.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>

using nx::vms::client::core::Geometry;

namespace {
// We request this size for thumbnails.
static const QSize kPreviewSize(640, 480);
}

namespace nx::vms::client::desktop {
namespace ui {

LayoutPreviewPainter::LayoutPreviewPainter(QObject* parent):
    base_type(parent),
    m_busyIndicator(new BusyIndicator())
{
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
            new LayoutThumbnailLoader(m_layout, previewSize,
                nx::api::ImageRequest::kLatestThumbnail.count(),
                /*skipExportPermissionCheck*/ true));

        connect(m_layoutThumbnailProvider.get(), &ImageProvider::statusChanged,
            this, &LayoutPreviewPainter::at_updateThumbnailStatus);
        connect(m_layoutThumbnailProvider.get(), &ImageProvider::imageChanged,
            this, &LayoutPreviewPainter::at_updateThumbnailImage);

        // If layout is modified while preview painter is already created, newly added cameras'
        // status changes will be ignored.
        for (const auto& resource: layoutResources(layout))
        {
            connect(resource.get(), &QnResource::statusChanged,
                m_layoutThumbnailProvider.get(), &ImageProvider::loadAsync);
        }

        m_layoutThumbnailProvider->loadAsync();
    }
}

void LayoutPreviewPainter::paint(QPainter* painter, const QRect& paintRect)
{
    static const int kFrameWidth = 1;
    static const QColor kFrameColor = core::colorTheme()->color("dark1");
    static const QColor kBackgroundColor = core::colorTheme()->color("dark7");

    if (paintRect.isEmpty())
        return;

    if (!m_layout)
    {
        Style::paintCosmeticFrame(painter, paintRect, kFrameColor, kFrameWidth, 0);
        return;
    }
    painter->fillRect(paintRect, kBackgroundColor);

    Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;
    if (m_layoutThumbnailProvider)
        status = m_layoutThumbnailProvider->status();

    if (m_layout
        && !m_layoutThumbnail.isNull()
        && (status == Qn::ThumbnailStatus::Loading || status == Qn::ThumbnailStatus::Loaded))
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

    Style::paintCosmeticFrame(painter, paintRect, kFrameColor, kFrameWidth, 0);
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
