#include "layout_preview_painter.h"

#include <camera/camera_thumbnail_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/style/skin.h>

#include <utils/common/scoped_painter_rollback.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

LayoutPreviewPainter::LayoutPreviewPainter(
    QnCameraThumbnailManager* thumbnailManager,
    QObject* parent)
    :
    base_type(parent),
    m_thumbnailManager(thumbnailManager),
    m_frameColor(Qt::black),
    m_backgroundColor(Qt::darkGray)
{
    NX_EXPECT(thumbnailManager);
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
    m_layout = layout;
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

void LayoutPreviewPainter::paint(QPainter* painter, const QRect& paintRect)
{
    static const int kFrameWidth = 1;

    if (paintRect.isEmpty())
        return;

    QnNxStyle::paintCosmeticFrame(painter, paintRect, m_frameColor, kFrameWidth, 0);

    if (!m_layout)
        return;

    QRect rect(paintRect);

    rect.adjust(kFrameWidth, kFrameWidth, -kFrameWidth, -kFrameWidth);
    painter->fillRect(paintRect, m_backgroundColor);

    //TODO: #GDM #3.1 paint layout background and calculate its size in bounding geometry
    QRectF bounding;
    for(const auto& data: m_layout->getItems())
    {
        QRectF itemRect = data.combinedGeometry;
        if (!itemRect.isValid())
            continue; //TODO: #GDM #VW some items can be not placed yet, wtf
        bounding = bounding.united(itemRect);
    }

    if (bounding.isEmpty())
        return;

    qreal space = m_layout->cellSpacing() * 0.5;

    qreal cellAspectRatio = m_layout->hasCellAspectRatio()
        ? m_layout->cellAspectRatio()
        : qnGlobals->defaultLayoutCellAspectRatio();

    qreal xscale, yscale, xoffset, yoffset;
    qreal sourceAr = cellAspectRatio * bounding.width() / bounding.height();

    qreal targetAr = paintRect.width() / paintRect.height();
    if (sourceAr > targetAr)
    {
        xscale = paintRect.width() / bounding.width();
        yscale = xscale / cellAspectRatio;
        xoffset = paintRect.left();

        qreal h = bounding.height() * yscale;
        yoffset = (paintRect.height() - h) * 0.5 + paintRect.top();
    }
    else
    {
        yscale = paintRect.height() / bounding.height();
        xscale = yscale * cellAspectRatio;
        yoffset = paintRect.top();

        qreal w = bounding.width() * xscale;
        xoffset = (paintRect.width() - w) * 0.5 + paintRect.left();
    }

    bool allItemsAreLoaded = true;
    foreach(const QnLayoutItemData &data, m_layout->getItems())
    {
        QRectF cellRect = data.combinedGeometry;
        if (!cellRect.isValid())
            continue;

        // cell bounds
        qreal x1 = (cellRect.left() - bounding.left() + space) * xscale + xoffset;
        qreal y1 = (cellRect.top() - bounding.top() + space) * yscale + yoffset;
        qreal w1 = (cellRect.width() - space * 2) * xscale;
        qreal h1 = (cellRect.height() - space * 2) * yscale;

        QRectF itemRect(x1, y1, w1, h1);
        QnNxStyle::paintCosmeticFrame(painter, itemRect, m_frameColor, kFrameWidth, 0);
        if (!paintItem(painter, itemRect, data))
            allItemsAreLoaded = false;
    }

//     const auto overlay = allItemsAreLoaded ? Qn::EmptyOverlay : Qn::LoadingOverlay;
//     m_statusOverlayController->setStatusOverlay(overlay, true);
}

bool LayoutPreviewPainter::paintItem(QPainter* painter, const QRectF& itemRect,
    const QnLayoutItemData& item)
{
    // Return true in all invalid cases to avoid "Loading..." overlay when it is not needed anyway
    NX_EXPECT(m_layout && m_layout->resourcePool());
    if (!m_layout || !m_layout->resourcePool())
        return true;

    if (!m_thumbnailManager)
        return true;

    const auto resource = m_layout->resourcePool()->getResourceByDescriptor(item.resource);
    if (!resource)
        return true;

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();

    auto drawFixedThumbnail = [painter, &itemRect](const QPixmap& thumb)
        {
            auto ar = QnGeometry::aspectRatio(thumb.size());
            auto rect = QnGeometry::expanded(ar, itemRect, Qt::KeepAspectRatio).toRect();
            painter->drawPixmap(rect, thumb);
        };

    if (resource->hasFlags(Qn::server))
    {
        drawFixedThumbnail(qnSkin->pixmap("item_placeholders/videowall_server_placeholder.png"));
    }
    else if (resource->hasFlags(Qn::web_page))
    {
        drawFixedThumbnail(qnSkin->pixmap("item_placeholders/videowall_webpage_placeholder.png"));
    }
    else if (resource->hasFlags(Qn::local_media))
    {
        drawFixedThumbnail(qnSkin->pixmap("item_placeholders/videowall_local_placeholder.png"));
    }
    else if (m_thumbnailManager->hasThumbnailForCamera(camera))
    {
        QPixmap pixmap = QPixmap::fromImage(m_thumbnailManager->thumbnailForCamera(camera));
        QSize mediaLayout = camera->getVideoLayout()->size();
        // ensure width and height are not zero
        mediaLayout.setWidth(qMax(mediaLayout.width(), 1));
        mediaLayout.setHeight(qMax(mediaLayout.height(), 1));

        QRectF sourceRect(0, 0, pixmap.width() * mediaLayout.width(),
            pixmap.height() * mediaLayout.height());

        auto drawPixmap =
            [painter, &pixmap, &mediaLayout](const QRectF &targetRect)
            {
                int width = targetRect.width() / mediaLayout.width();
                int height = targetRect.height() / mediaLayout.height();
                for (int i = 0; i < mediaLayout.width(); ++i)
                {
                    for (int j = 0; j < mediaLayout.height(); ++j)
                    {
                        painter->drawPixmap(QRectF(targetRect.left() + width * i,
                            targetRect.top() + height * j, width, height).toRect(), pixmap);
                    }
                }
            };

        if (!qFuzzyIsNull(item.rotation))
        {
            QnScopedPainterTransformRollback guard(painter); Q_UNUSED(guard);
            painter->translate(itemRect.center());
            painter->rotate(item.rotation);
            painter->translate(-itemRect.center());
            drawPixmap(QnGeometry::encloseRotatedGeometry(itemRect, QnGeometry::aspectRatio(sourceRect), item.rotation));
        }
        else
        {
            drawPixmap(itemRect);
        }
        return true;
    }

    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        m_thumbnailManager->selectCamera(camera);
        return false;
    }

    return true;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

