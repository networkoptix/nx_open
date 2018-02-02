#include "area_tooltip_item.h"

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

static const qreal kRoundingRadius = 2.0;
static const QMarginsF kTooltipMargins(6, 4, 6, 4);

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

    return QRectF(QPoint(), Geometry::dilated(d->textPixmap.size(), kTooltipMargins));
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
        QnScopedPainterBrushRollback brushRollback(painter, d->backgroundColor);
        QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
        QnScopedPainterAntialiasingRollback(painter, true);

        const ui::PainterTransformScaleStripper scaleStripper(painter);

        painter->drawRoundedRect(
            scaleStripper.mapRect(boundingRect()), kRoundingRadius, kRoundingRadius);
    }

    paintPixmapSharp(
        painter, d->textPixmap, QPointF(kTooltipMargins.left(), kTooltipMargins.top()));
}

} // namespace desktop
} // namespace client
} // namespace nx
