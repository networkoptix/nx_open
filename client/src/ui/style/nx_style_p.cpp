#include "nx_style_p.h"

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>

using namespace style;

QnPaletteColor QnNxStylePrivate::findColor(const QColor &color) const
{
    return palette.color(color);
}

QnPaletteColor QnNxStylePrivate::mainColor(QnNxStyle::Colors::Palette palette) const
{
    int index = -1;

    switch (palette)
    {
    case QnNxStyle::Colors::kBase:
        index = 6;
        break;
    case QnNxStyle::Colors::kContrast:
        index = 7;
        break;
    case QnNxStyle::Colors::kRed:
    case QnNxStyle::Colors::kBlue:
    case QnNxStyle::Colors::kBrand:
        index = 4;
        break;
    case QnNxStyle::Colors::kGreen:
        index = 3;
        break;
    }

    if (index < 0)
        return QnPaletteColor();

    return this->palette.color(QnNxStyle::Colors::paletteName(palette), index);
}

QColor QnNxStylePrivate::checkBoxColor(const QStyleOption *option, bool radio) const
{
    QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Text));
    QColor color = mainColor.darker(6);

    if (option->state.testFlag(QStyle::State_Off))
    {
        if (option->state.testFlag(QStyle::State_MouseOver) ||
            option->state.testFlag(QStyle::State_HasFocus))
        {
            color = mainColor.darker(4);
        }
    }
    else if (option->state.testFlag(QStyle::State_On) ||
             option->state.testFlag(QStyle::State_NoChange))
    {
        if (!radio && (option->state.testFlag(QStyle::State_MouseOver) ||
                       option->state.testFlag(QStyle::State_HasFocus)))
        {
            color = mainColor.lighter(3);
        }
        else
        {
            color = mainColor;
        }
    }

    return color;
}

void QnNxStylePrivate::drawSwitch(
        QPainter *painter,
        const QStyleOption *option,
        const QWidget *widget) const
{
    Q_UNUSED(widget)

    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool standalone = option->state.testFlag(QStyle::State_Item);

    int colorShift = 0;
    if (option->state.testFlag(QStyle::State_Sunken))
    {
        colorShift = -1;
    }
    else if (option->state.testFlag(QStyle::State_MouseOver) ||
             option->state.testFlag(QStyle::State_HasFocus))
    {
        colorShift = 1;
    }

    QnPaletteColor backgroundColor = findColor(option->palette.button().color()).lighter(colorShift);
    QnPaletteColor gripColor = findColor(option->palette.window().color());
    QnPaletteColor highlightColor = mainColor(QnNxStyle::Colors::kGreen).lighter(colorShift);

    bool drawTop = true;
    bool drawBottom = true;

    //TODO: Implement animation

    qreal animationProgress = checked ? 1.0 : 0.0;

    if (qFuzzyIsNull(animationProgress))
        drawTop = false;
    else if (qFuzzyEquals(animationProgress, 1.0))
        drawBottom = false;

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);

    QRectF rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, Metrics::kSwitchSize, option->rect);
    rect = QnGeometry::eroded(rect, 0.5);

    QRectF indicatorsRect = QnGeometry::eroded(rect, dp(3));

    if (drawBottom)
    {
        painter->setPen(standalone ? backgroundColor : backgroundColor.darker(1));
        painter->setBrush(backgroundColor.color());
        painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

        QRectF zeroRect = indicatorsRect;
        zeroRect.setLeft(indicatorsRect.right() - indicatorsRect.height());

        painter->setPen(QPen(backgroundColor.lighter(2).color(), dp(2)));
        painter->drawRoundedRect(zeroRect, zeroRect.width() / 2, zeroRect.height() / 2);
    }

    if (drawTop)
    {
        painter->setPen(highlightColor);
        painter->setBrush(highlightColor.color());
        painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

        painter->setPen(QPen(highlightColor.lighter(2).color(), dp(2)));

        int x = indicatorsRect.left() + indicatorsRect.height() / 2 + dp(1);
        int h = indicatorsRect.height() - dp(3);
        int y = rect.center().y() - h / 2;
        painter->drawLine(x, y, x, y + h);
    }

    QRectF handleRect = QnGeometry::eroded(rect, dp(1));
    handleRect.moveLeft(handleRect.left() + (handleRect.width() - handleRect.height()) * animationProgress);
    handleRect.setWidth(handleRect.height());

    painter->setPen(Qt::NoPen);
    painter->setBrush(gripColor.color());
    painter->drawRoundedRect(handleRect, handleRect.width() / 2, handleRect.height() / 2);
}

void QnNxStylePrivate::drawCheckBox(QPainter *painter, const QStyleOption *option, const QWidget *widget) const
{
    Q_UNUSED(widget)

    QPen pen(checkBoxColor(option));
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setCapStyle(Qt::FlatCap);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);

    const int size = Metrics::kCheckIndicatorSize - dp(4);

    QRectF rect = aligned(QSize(size, size), option->rect, Qt::AlignCenter);

    RectCoordinates rc(rect);

    if (option->state.testFlag(QStyle::State_On))
    {
        QPainterPath path;
        path.moveTo(rc.x(0.7), rc.y(0));
        path.lineTo(rc.x(0), rc.y(0));
        path.lineTo(rc.x(0), rc.y(1));
        path.lineTo(rc.x(1), rc.y(1));
        path.lineTo(rc.x(1), rc.y(0.6));

        painter->setPen(pen);
        painter->drawPath(path);

        path = QPainterPath();
        path.moveTo(rc.x(0.2), rc.y(0.45));
        path.lineTo(rc.x(0.5), rc.y(0.75));
        path.lineTo(rc.x(1.05), rc.y(0.15));

        pen.setWidthF(dp(2));
        painter->setPen(pen);

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

        painter->drawPath(path);
    }
    else
    {
        painter->drawRect(rect);

        if (option->state.testFlag(QStyle::State_NoChange))
        {
            pen.setWidth(2);
            painter->setPen(pen);
            painter->drawLine(QPointF(rc.x(0.2), rc.y(0.5)), QPointF(rc.x(0.9), rc.y(0.5)));
        }
    }
}

void QnNxStylePrivate::drawRadioButton(QPainter *painter, const QStyleOption *option, const QWidget *widget) const
{
    Q_UNUSED(widget)

    QPen pen(checkBoxColor(option, true), 1.2);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    QRect rect = option->rect.adjusted(1, 1, -2, -2);

    painter->drawEllipse(rect);

    if (option->state.testFlag(QStyle::State_On))
    {
        painter->setBrush(pen.brush());
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
    }
    else if (option->state.testFlag(QStyle::State_NoChange))
    {
        pen.setWidth(2);
        painter->setPen(pen);
        painter->drawLine(QPointF(rect.left() + 4, rect.top() + rect.height() / 2.0),
                          QPointF(rect.right() - 3, rect.top() + rect.height() / 2.0));
    }
}

void QnNxStylePrivate::drawSortIndicator(QPainter *painter, const QStyleOption *option, const QWidget *widget) const
{
    Q_UNUSED(widget)

    bool down = true;
    if (const QStyleOptionHeader *header =
            qstyleoption_cast<const QStyleOptionHeader *>(option))
    {
        if (header->sortIndicator == QStyleOptionHeader::None)
            return;

        down = header->sortIndicator == QStyleOptionHeader::SortDown;
    }

    QColor color = option->palette.brightText().color();

    int w = dp(4);
    int h = dp(2);
    int y = option->rect.top() + dp(3);
    int x = option->rect.x() + dp(1);
    int wstep = dp(4);
    int ystep = dp(4);

    if (!down)
    {
        w += 2 * wstep;
        wstep = -wstep;
    }

    for (int i = 0; i < 3; ++i)
    {
        painter->fillRect(x, y, w, h, color);
        w += wstep;
        y += ystep;
    }
}

void QnNxStylePrivate::drawCross(
        QPainter* painter,
        const QRect& rect,
        const QColor& color) const
{
    const QSizeF crossSize(Metrics::kCrossSize, Metrics::kCrossSize);
    QRectF crossRect = aligned(crossSize, rect);

    QPen pen(color, 1.5, Qt::SolidLine, Qt::FlatCap);
    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawLine(crossRect.topLeft(), crossRect.bottomRight());
    painter->drawLine(crossRect.topRight(), crossRect.bottomLeft());
}
