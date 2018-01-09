#include "layout_preview_painter.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/common/palette.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/style/nx_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/busy_indicator.h>
#include <ui/widgets/common/autoscaled_plain_text.h>
#include <ui/workaround/sharp_pixmap_painting.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_manager.h>

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
    m_backgroundColor(Qt::darkGray),
    m_itemBackgroundColor(Qt::black),
    m_fontColor(Qt::white),
    m_busyIndicator(new QnBusyIndicator())
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

    // TODO: #GDM #3.1 paint layout background and calculate its size in bounding geometry
    QRectF bounding;
    for(const auto& data: m_layout->getItems())
    {
        QRectF itemRect = data.combinedGeometry;
        if (!itemRect.isValid())
            continue; // TODO: #GDM #VW some items can be not placed yet, wtf
        bounding = bounding.united(itemRect);
    }

    if (bounding.isEmpty())
        return;

    // Handle negative spacing for exported layouts.
    qreal space = std::max(0.0, m_layout->cellSpacing() * 0.5);

    qreal cellAspectRatio = m_layout->hasCellAspectRatio()
        ? m_layout->cellAspectRatio()
        : qnGlobals->defaultLayoutCellAspectRatio();

    qreal xscale, yscale, xoffset, yoffset;
    qreal sourceAr = cellAspectRatio * bounding.width() / bounding.height();

    QRect contentsRect(paintRect);
    contentsRect.adjust(-kFrameWidth, -kFrameWidth, kFrameWidth, kFrameWidth);
    qreal targetAr = QnGeometry::aspectRatio(contentsRect);
    if (sourceAr > targetAr)
    {
        xscale = contentsRect.width() / bounding.width();
        yscale = xscale / cellAspectRatio;
        xoffset = contentsRect.left();

        qreal h = bounding.height() * yscale;
        yoffset = (contentsRect.height() - h) * 0.5 + contentsRect.top();
    }
    else
    {
        yscale = contentsRect.height() / bounding.height();
        xscale = yscale * cellAspectRatio;
        yoffset = contentsRect.top();

        qreal w = bounding.width() * xscale;
        xoffset = (contentsRect.width() - w) * 0.5 + contentsRect.left();
    }

    for (const auto& data: m_layout->getItems())
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
        paintItem(painter, QnGeometry::eroded(itemRect, 1), data);
    }

    QnNxStyle::paintCosmeticFrame(painter, paintRect, m_frameColor, kFrameWidth, 0);
}

LayoutPreviewPainter::ThumbnailInfo LayoutPreviewPainter::thumbnailForItem(
    const QnLayoutItemData& item) const
{
    NX_EXPECT(m_layout);
    if (!m_layout)
        return ThumbnailInfo();

    auto resourcePool = m_layout->resourcePool();
    if (!resourcePool)
        resourcePool = m_thumbnailManager->resourcePool();
    NX_EXPECT(resourcePool);
    if (!resourcePool)
        return ThumbnailInfo();

    const auto resource = resourcePool->getResourceByDescriptor(item.resource);
    if (!resource)
        return ThumbnailInfo();

    // TODO: #GDM #rename placeholders
    if (resource->hasFlags(Qn::server))
        return qnSkin->pixmap("item_placeholders/videowall_server_placeholder.png");

    if (resource->hasFlags(Qn::web_page))
        return qnSkin->pixmap("item_placeholders/videowall_webpage_placeholder.png");

    if (resource->hasFlags(Qn::local_media))
        return qnSkin->pixmap("item_placeholders/videowall_local_placeholder.png");

    if (!m_thumbnailManager)
        return ThumbnailInfo();

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    NX_EXPECT(camera);

    // TODO: #GDM remove m_thumbnailManager->select and thumbnailReady?
    if (!m_thumbnailManager->hasThumbnailForCamera(camera))
        m_thumbnailManager->selectCamera(camera);

    return ThumbnailInfo{
        m_thumbnailManager->statusForCamera(camera),
        false,
        QPixmap::fromImage(m_thumbnailManager->thumbnailForCamera(camera))};
}

void LayoutPreviewPainter::paintItem(QPainter* painter, const QRectF& itemRect,
    const QnLayoutItemData& item)
{
    const auto info = thumbnailForItem(item);
    if (!info.pixmap.isNull())
    {
        auto ar = QnGeometry::aspectRatio(info.pixmap.size());
        auto rect = QnGeometry::expanded(ar, itemRect, Qt::KeepAspectRatio).toRect();

        if (info.ignoreTrasformation)
        {
            painter->drawPixmap(rect, info.pixmap);
        }
        else
        {
            auto drawPixmap =
                [painter, &info, zoomRect = item.zoomRect](const QRect& targetRect)
                {
                    if (zoomRect.isNull())
                    {
                        painter->drawPixmap(targetRect, info.pixmap);
                    }
                    else
                    {
                        painter->drawPixmap(targetRect, info.pixmap,
                            QnGeometry::subRect(info.pixmap.rect(), zoomRect).toRect());
                    }
                };

            if (!qFuzzyIsNull(item.rotation))
            {
                QnScopedPainterTransformRollback guard(painter); Q_UNUSED(guard);
                painter->translate(itemRect.center());
                painter->rotate(item.rotation);
                painter->translate(-itemRect.center());
                const auto targetRect = QnGeometry::encloseRotatedGeometry(rect,
                    QnGeometry::aspectRatio(info.pixmap.size()), item.rotation);

                drawPixmap(targetRect.toRect());
            }
            else
            {
                drawPixmap(rect);
            }
        }
    }
    else // Pixmap is absent
    {
        switch (info.status)
        {
            case Qn::ThumbnailStatus::Loaded:
                NX_EXPECT(false);
                //fall-through
            case Qn::ThumbnailStatus::Refreshing:
            case Qn::ThumbnailStatus::Invalid:
            case Qn::ThumbnailStatus::NoData:
            {
                static const int kMargin = 4;

                QScopedPointer<QnAutoscaledPlainText> pw(new QnAutoscaledPlainText());
                pw->setText(tr("NO DATA"));
                pw->setProperty(style::Properties::kDontPolishFontProperty, true);
                pw->setAlignment(Qt::AlignCenter);
                pw->setGeometry(QnGeometry::eroded(itemRect.toRect(), kMargin));
                setPaletteColor(pw.data(), QPalette::Window, m_itemBackgroundColor);

                const int devicePixelRatio = painter->device()->devicePixelRatio();

                /* Paint into sub-cache: */
                QPixmap pixmap(itemRect.size().toSize() * devicePixelRatio);
                pixmap.setDevicePixelRatio(devicePixelRatio);
                pixmap.fill(Qt::transparent);
                pw->render(&pixmap);

                paintPixmapSharp(painter, pixmap, itemRect.topLeft() + QPoint(kMargin, kMargin));

                break;
            }

            case Qn::ThumbnailStatus::Loading:
            {
                static const qreal kFillFactor = 0.5;
                const auto busyRect = QnGeometry::expanded(
                    QnGeometry::aspectRatio(m_busyIndicator->size()),
                    itemRect.size() * kFillFactor,
                    itemRect.center(),
                    Qt::KeepAspectRatio);

                qreal scaleFactor = QnGeometry::scaleFactor(m_busyIndicator->size(),
                    busyRect.size(), Qt::KeepAspectRatio);

                QnScopedPainterTransformRollback transformRollback(painter);
                QnScopedPainterBrushRollback brushRollback(painter, m_fontColor);
                QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
                QnScopedPainterAntialiasingRollback aaRollback(painter, true);

                painter->translate(busyRect.topLeft());
                painter->scale(scaleFactor, scaleFactor);
                m_busyIndicator->paint(painter);
                break;
            }
            default:
                break;
        }
    }
}

LayoutPreviewPainter::ThumbnailInfo::ThumbnailInfo():
    status(Qn::ThumbnailStatus::Invalid),
    ignoreTrasformation(true)
{
}

LayoutPreviewPainter::ThumbnailInfo::ThumbnailInfo(const QPixmap& pixmap):
    status(Qn::ThumbnailStatus::Loaded),
    ignoreTrasformation(true),
    pixmap(pixmap)
{
}

LayoutPreviewPainter::ThumbnailInfo::ThumbnailInfo(
    Qn::ThumbnailStatus status,
    bool ignoreTrasformation,
    const QPixmap& pixmap)
    :
    status(status),
    ignoreTrasformation(ignoreTrasformation),
    pixmap(pixmap)
{
}


} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

