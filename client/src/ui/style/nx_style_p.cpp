#include "nx_style_p.h"

#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/math/fuzzy.h>

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
    case QnNxStyle::Colors::kYellow:
        index = 0;
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
        if (option->state.testFlag(QStyle::State_MouseOver))
        {
            color = mainColor.darker(4);
        }
    }
    else if (option->state.testFlag(QStyle::State_On) ||
             option->state.testFlag(QStyle::State_NoChange))
    {
        if (!radio && (option->state.testFlag(QStyle::State_MouseOver)))
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
    Q_UNUSED(widget);

    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool standalone = !qstyleoption_cast<const QStyleOptionButton*>(option);

    QColor fillColorOn;     // On: background
    QColor fillColorOff;    // Off: background
    QColor frameColorOn;    // On: frame
    QColor frameColorOff;   // Off: frame
    QColor signColorOn;     // On: "1" indicator
    QColor signColorOff;    // Off: "0" indicator

    QnPaletteColor mainGreen = mainColor(QnNxStyle::Colors::kGreen);
    QnPaletteColor mainDark = mainColor(QnNxStyle::Colors::kBase);

    if (standalone)
    {
        bool hovered = option->state.testFlag(QStyle::State_MouseOver);

        fillColorOff = mainDark.lighter(hovered ? 7 : 6);
        fillColorOn = mainGreen.lighter(hovered ? 1 : 0);
        frameColorOff = fillColorOff;
        frameColorOn = fillColorOn;
        signColorOff = mainDark.lighter(10);
        signColorOn = mainGreen.lighter(hovered ? 3 : 2);
    }
    else
    {
        fillColorOff = mainDark.lighter(1);
        fillColorOn = mainGreen;
        frameColorOff = mainDark;
        frameColorOn = fillColorOn;
        signColorOff = mainDark.lighter(3);
        signColorOn = mainGreen.lighter(2);
    }

    bool drawTop = true;
    bool drawBottom = true;

    //TODO: Implement animation

    qreal animationProgress = checked ? 1.0 : 0.0;

    if (qFuzzyIsNull(animationProgress))
        drawTop = false;
    else if (qFuzzyEquals(animationProgress, 1.0))
        drawBottom = false;

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterClipPathRollback clipRollback(painter);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    QSize switchSize = standalone ? Metrics::kStandaloneSwitchSize : Metrics::kButtonSwitchSize;

    QRectF rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, switchSize, option->rect);
    rect = QnGeometry::eroded(rect, 0.5);

    QRectF indicatorsRect = QnGeometry::eroded(rect, 3.5);

    /* Set clip path with excluded grip circle: */
    QRectF gripRect = QnGeometry::eroded(rect, dp(1));
    gripRect.moveLeft(gripRect.left() + (gripRect.width() - gripRect.height()) * animationProgress);
    gripRect.setWidth(gripRect.height());
    QPainterPath wholeRect;
    wholeRect.addRect(option->rect);
    QPainterPath gripCircle;
    gripCircle.addEllipse(QnGeometry::dilated(gripRect, 0.5));
    painter->setClipPath(wholeRect.subtracted(gripCircle));

    if (drawBottom)
    {
        painter->setPen(QPen(frameColorOff, 0));
        painter->setBrush(fillColorOff);
        painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

        QRectF zeroRect = indicatorsRect;
        zeroRect.setLeft(indicatorsRect.right() - indicatorsRect.height());

        painter->setPen(QPen(signColorOff, dp(2)));
        painter->drawRoundedRect(zeroRect, zeroRect.width() / 2, zeroRect.height() / 2);
    }

    if (drawTop)
    {
        painter->setPen(QPen(frameColorOn, 0));
        painter->setBrush(fillColorOn);
        painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

        painter->setPen(QPen(signColorOn, dp(2)));

        int x = indicatorsRect.left() + indicatorsRect.height() / 2 + dp(1);
        int h = indicatorsRect.height() - dp(3);
        int y = rect.center().y() - h / 2;
        painter->drawLine(x, y, x, y + h + 1);
    }

    if (standalone && option->state.testFlag(QStyle::State_HasFocus))
    {
        Q_Q(const QnNxStyle);
        QStyleOptionFocusRect focusOption;
        focusOption.QStyleOption::operator=(*option);
        q->proxy()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOption, painter, widget);
    }
}

void QnNxStylePrivate::drawCheckBox(QPainter *painter, const QStyleOption *option, const QWidget *widget) const
{
    Q_UNUSED(widget)

    QPen pen(checkBoxColor(option));
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setCapStyle(Qt::FlatCap);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
    QnScopedPainterAntialiasingRollback aaRollback(painter, true);

    const int size = Metrics::kCheckIndicatorSize - dp(5);
    QRect rect = aligned(QSize(size, size), option->rect, Qt::AlignCenter);

    QRectF aaAlignedRect(rect);
    aaAlignedRect.adjust(0.5, 0.5, 0.5, 0.5);

    RectCoordinates rc(rect);
    if (option->state.testFlag(QStyle::State_On))
    {
        QRect clipRect(rect.adjusted(0, 0, 1, 1));
        QRect excludeRect(clipRect);
        excludeRect.setHeight(excludeRect.height() / 2);
        excludeRect.setLeft(excludeRect.right() - excludeRect.width() / 5);
        {
            QnScopedPainterClipRegionRollback clipRollback(painter, QRegion(clipRect).subtracted(excludeRect));
            painter->drawRoundedRect(aaAlignedRect, Metrics::kCheckboxCornerRadius, Metrics::kCheckboxCornerRadius);
        }

        QPainterPath path;
        path.moveTo(rc.x(0.25), rc.y(0.38));
        path.lineTo(rc.x(0.55), rc.y(0.70));
        path.lineTo(rc.x(1.12), rc.y(0.13));

        pen.setWidthF(dp(2));
        painter->setPen(pen);

        painter->drawPath(path);
    }
    else
    {
        painter->drawRoundedRect(aaAlignedRect, Metrics::kCheckboxCornerRadius, Metrics::kCheckboxCornerRadius);

        if (option->state.testFlag(QStyle::State_NoChange))
        {
            pen.setWidthF(dp(2));
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
