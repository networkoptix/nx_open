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
