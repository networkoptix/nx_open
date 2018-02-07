#include "area_tooltip_item.h"

#include <array>

#include <QtGui/QPainter>

#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/desktop/ui/graphics/painters/highlighted_area_text_painter.h>
#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr qreal kRoundingRadius = 2;
static constexpr QSizeF kArrowSize(8, 4);
static constexpr qreal kArrowMargin = 6;
static constexpr QMarginsF kTooltipMargins(6, 4, 6, 4);

struct ArrowPosition
{
    static constexpr int kNoEdge = 0;

    int edge = kNoEdge;
    QPointF position;
};

ArrowPosition arrowPosition(const QRectF& tooltipRect, const QRectF& objectRect)
{
    const auto tooltipCenter = tooltipRect.center();
    ArrowPosition arrowPos;

    if (tooltipCenter.x() > objectRect.right())
    {
        arrowPos.edge = Qt::LeftEdge;
        arrowPos.position.setX(tooltipRect.left() - kArrowSize.width() / 2 + 1);
    }
    else if (tooltipCenter.x() < objectRect.left())
    {
        arrowPos.edge = Qt::RightEdge;
        arrowPos.position.setX(tooltipRect.right() + kArrowSize.width() / 2 - 1);
    }
    else if (tooltipCenter.y() > objectRect.bottom())
    {
        arrowPos.edge = Qt::TopEdge;
        arrowPos.position.setY(tooltipRect.top() - kArrowSize.height() / 2 - 1);
    }
    else if (tooltipCenter.y() < objectRect.top())
    {
        arrowPos.edge = Qt::BottomEdge;
        arrowPos.position.setY(tooltipRect.bottom() + kArrowSize.height() / 2 + 1);
    }

    constexpr qreal margin = kArrowMargin + kArrowSize.width() / 2;

    switch (arrowPos.edge)
    {
        case Qt::LeftEdge:
        case Qt::RightEdge:
            arrowPos.position.setY(std::max(tooltipRect.top(), objectRect.top()) + margin);
            break;
        case Qt::TopEdge:
        case Qt::BottomEdge:
            arrowPos.position.setX(std::max(tooltipRect.left(), objectRect.left()) + margin);
            break;
        default:
            break;
    }

    return arrowPos;
}

void paintArrow(QPainter* painter, const QRectF& rect, const Qt::Edge edge)
{
    QPainterPath path;

    std::array<QPointF, 4> points{{
        rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()
    }};

    auto point = [&points](size_t i) { return points[i % points.size()]; };

    size_t firstCorner = 0;
    switch (edge)
    {
        case Qt::BottomEdge:
            firstCorner = 2;
            break;
        case Qt::LeftEdge:
            firstCorner = 3;
            break;
        case Qt::RightEdge:
            firstCorner = 1;
            break;
        default:
            break;
    }

    const auto tip = (point(firstCorner) + point(firstCorner + 1)) / 2;
    path.moveTo(tip);
    path.lineTo(point(firstCorner + 2));
    path.lineTo(point(firstCorner + 3));
    path.lineTo(tip);

    painter->drawPath(path);
}

} // namespace

using nx::client::core::Geometry;

class AreaTooltipItem::Private
{
public:
    void updateTextPixmap();
    void invalidateTextPixmap();

public:
    QString text;
    QColor backgroundColor;
    QRectF targetObjectGeometry;

    QPixmap textPixmap;
    HighlightedAreaTextPainter textPainter;
};

void AreaTooltipItem::Private::updateTextPixmap()
{
    if (!textPixmap.isNull())
        return;

    textPixmap = textPainter.paintText(text);
}

void AreaTooltipItem::Private::invalidateTextPixmap()
{
    textPixmap = QPixmap();
}

//-------------------------------------------------------------------------------------------------

AreaTooltipItem::AreaTooltipItem(QGraphicsItem* parent):
    base_type(parent),
    d(new Private())
{
}

AreaTooltipItem::~AreaTooltipItem()
{
}

QString AreaTooltipItem::text() const
{
    return d->text;
}

void AreaTooltipItem::setText(const QString& text)
{
    if (d->text == text)
        return;

    prepareGeometryChange();

    d->text = text;
    d->invalidateTextPixmap();
}

QFont AreaTooltipItem::font() const
{
    return d->textPainter.font();
}

void AreaTooltipItem::setFont(const QFont& font)
{
    if (d->textPainter.font() == font)
        return;

    prepareGeometryChange();

    d->textPainter.setFont(font);
    d->invalidateTextPixmap();
}

QColor AreaTooltipItem::textColor() const
{
    return d->textPainter.color();
}

void AreaTooltipItem::setTextColor(const QColor& color)
{
    if (d->textPainter.color() == color)
        return;

    d->textPainter.setColor(color);
    d->invalidateTextPixmap();
    update();
}

QColor AreaTooltipItem::backgroundColor() const
{
    return d->backgroundColor;
}

void AreaTooltipItem::setBackgroundColor(const QColor& color)
{
    if (d->backgroundColor == color)
        return;

    d->backgroundColor = color;
    update();
}

QRectF AreaTooltipItem::boundingRect() const
{
    d->updateTextPixmap();

    if (d->textPixmap.isNull())
        return QRectF();

    return QRectF(QPoint(), Geometry::dilated(d->textPixmap.size(), textMargins()));
}

void AreaTooltipItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    d->updateTextPixmap();

    if (d->textPixmap.isNull())
        return;

    if (d->backgroundColor.isValid())
    {
        const auto rect = Geometry::eroded(boundingRect(), kArrowSize.height());

        QnScopedPainterBrushRollback brushRollback(painter, d->backgroundColor);
        QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
        QnScopedPainterAntialiasingRollback(painter, true);

        const ui::PainterTransformScaleStripper scaleStripper(painter);

        painter->drawRoundedRect(scaleStripper.mapRect(rect), kRoundingRadius, kRoundingRadius);

        const auto arrowPos = arrowPosition(rect.translated(pos()), d->targetObjectGeometry);
        if (arrowPos.edge != ArrowPosition::kNoEdge)
        {
            const QSizeF arrowSize =
                (arrowPos.edge == Qt::TopEdge || arrowPos.edge == Qt::BottomEdge)
                    ? kArrowSize
                    : QSizeF(kArrowSize.height(), kArrowSize.width());

            const QRectF arrowRect = scaleStripper.mapRect(QRectF(
                arrowPos.position - pos() - Geometry::toPoint(arrowSize / 2),
                arrowSize));

            paintArrow(painter, arrowRect, static_cast<Qt::Edge>(arrowPos.edge));
        }
    }

    paintPixmapSharp(
        painter,
        d->textPixmap,
        QPointF(
            kTooltipMargins.left() + kArrowSize.height(),
                    kTooltipMargins.top() + kArrowSize.height()));
}

QMarginsF AreaTooltipItem::textMargins() const
{
    return QMarginsF(
        kTooltipMargins.left() + kArrowSize.height(),
        kTooltipMargins.top() + kArrowSize.height(),
        kTooltipMargins.right() + kArrowSize.height(),
        kTooltipMargins.bottom() + kArrowSize.height());
}

QRectF AreaTooltipItem::targetObjectGeometry() const
{
    return d->targetObjectGeometry;
}

void AreaTooltipItem::setTargetObjectGeometry(const QRectF& geometry)
{
    if (d->targetObjectGeometry == geometry)
        return;

    d->targetObjectGeometry = geometry;
    update();
}

} // namespace desktop
} // namespace client
} // namespace nx
