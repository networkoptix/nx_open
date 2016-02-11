#include "nx_style_p.h"

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <ui/common/geometry.h>

using namespace style;

QnPaletteColor QnNxStylePrivate::findColor(const QColor &color) const
{
    return palette.color(color);
}

QnPaletteColor QnNxStylePrivate::mainColor(style::Colors::Palette palette) const
{
    int index = -1;

    switch (palette)
    {
    case Colors::kBase:
        index = 6;
        break;
    case Colors::kContrast:
        index = 7;
        break;
    case Colors::kBlue:
    case Colors::kBrang:
        index = 4;
        break;
    case Colors::kGreen:
        index = 3;
        break;
    }

    if (index < 0)
        return QnPaletteColor();

    return this->palette.color(Colors::paletteName(palette), index);
}

QColor QnNxStylePrivate::checkBoxColor(const QStyleOption *option, bool radio) const
{
    QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Text));
    QColor color = mainColor.darker(6);

    if (option->state & QStyle::State_Off)
    {
        if (option->state & QStyle::State_MouseOver)
            color = mainColor.darker(4);
    }
    else if (option->state & QStyle::State_On || option->state & QStyle::State_NoChange)
    {
        if (!radio && option->state & QStyle::State_MouseOver)
            color = mainColor.lighter(3);
        else
            color = mainColor;
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
        colorShift = -1;
    else if (option->state.testFlag(QStyle::State_MouseOver))
        colorShift = 1;

    QnPaletteColor backgroundColor = findColor(option->palette.button().color()).lighter(colorShift);
    QnPaletteColor gripColor = findColor(option->palette.window().color()).lighter(colorShift);
    QnPaletteColor highlightColor = mainColor(Colors::kGreen).lighter(colorShift);

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
    painter->setPen(pen);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);

    QRectF rect = option->rect.adjusted(2, 2, -3, -3);

    if (option->state.testFlag(QStyle::State_On))
    {
        QPainterPath path;
        path.moveTo(rect.right() - rect.width() * 0.3, rect.top());
        path.lineTo(rect.left(), rect.top());
        path.lineTo(rect.left(), rect.bottom());
        path.lineTo(rect.right(), rect.bottom());
        path.lineTo(rect.right(), rect.top() + rect.height() * 0.6);

        painter->setPen(pen);
        painter->drawPath(path);

        path = QPainterPath();
        path.moveTo(rect.left() + rect.width() * 0.2, rect.top() + rect.height() * 0.45);
        path.lineTo(rect.left() + rect.width() * 0.5, rect.top() + rect.height() * 0.75);
        path.lineTo(rect.right() + rect.width() * 0.05, rect.top() + rect.height() * 0.15);

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
            painter->drawLine(QPointF(rect.left() + 2, rect.top() + rect.height() / 2.0),
                              QPointF(rect.right() - 1, rect.top() + rect.height() / 2.0));
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
