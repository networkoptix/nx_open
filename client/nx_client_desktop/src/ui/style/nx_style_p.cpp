#include "nx_style_p.h"
#include "skin.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QScrollBar>

#include <limits>

#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/client/core/utils/geometry.h>

using namespace style;
using nx::client::core::Geometry;

namespace {

QWindow* qt_getWindow(const QWidget* widget)
{
    return widget ? widget->window()->windowHandle() : 0;
}

void paintLabelIcon(
    QRect* labelRect,
    QPainter* painter,
    const QIcon& icon,
    Qt::LayoutDirection direction,
    Qt::Alignment alignment,
    int padding,
    QIcon::Mode mode = QIcon::Normal,
    QIcon::State state = QIcon::Off)
{
    QSize iconSize = QnSkin::maximumSize(icon, mode, state);
    QRect iconRect = QStyle::alignedRect(
        direction,
        alignment,
        iconSize,
        *labelRect);

    icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);

    if (labelRect->left() == iconRect.left())
        labelRect->setLeft(iconRect.right() + padding + 1);
    else
        labelRect->setRight(iconRect.left() - padding - 1);
}

/* Workaround while Qt's QWidget::mapFromGlobal is broken: */

QPoint mapFromGlobal(const QGraphicsProxyWidget* to, const QPoint& globalPos);
QPoint mapFromGlobal(const QWidget* to, const QPoint& globalPos)
{
    if (auto proxied = QnNxStylePrivate::graphicsProxiedWidget(to))
    {
        return to->mapFrom(proxied, mapFromGlobal(
            proxied->graphicsProxyWidget(), globalPos));
    }

    return to->mapFromGlobal(globalPos);
}

QPoint mapFromGlobal(const QGraphicsProxyWidget* to, const QPoint& globalPos)
{
    static const QPoint kInvalidPos(
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max());

    auto scene = to->scene();
    if (!scene)
        return kInvalidPos;

    auto views = scene->views();
    if (views.empty())
        return kInvalidPos;

    auto viewPos = mapFromGlobal(views[0], globalPos);
    return to->mapFromScene(views[0]->mapToScene(viewPos)).toPoint();
}

} // namespace

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
    case QnNxStyle::Colors::kBrand:
        index = 7;
        break;
    case QnNxStyle::Colors::kRed:
    case QnNxStyle::Colors::kBlue:
        index = 4;
        break;
    case QnNxStyle::Colors::kGreen:
        index = 3;
        break;
    case QnNxStyle::Colors::kYellow:
        index = 2;
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

bool QnNxStylePrivate::isCheckableButton(const QStyleOption* option)
{
    if (qstyleoption_cast<const QStyleOptionButton*>(option))
    {
        if (option->state.testFlag(QStyle::State_On) ||
            option->state.testFlag(QStyle::State_Off) ||
            option->state.testFlag(QStyle::State_NoChange))
        {
            return true;
        }

        const QAbstractButton* buttonWidget = qobject_cast<const QAbstractButton*>(option->styleObject);
        if (buttonWidget && buttonWidget->isCheckable())
            return true;
    }

    return false;
}

bool QnNxStylePrivate::isTextButton(const QStyleOption* option)
{
    if (auto button = qstyleoption_cast<const QStyleOptionButton*>(option))
        return button->features.testFlag(QStyleOptionButton::Flat);

    return false;
}

void QnNxStylePrivate::drawSwitch(
    QPainter *painter,
    const QStyleOption *option,
    const QWidget *widget) const
{
    Q_UNUSED(widget);

    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool undefined = option->state.testFlag(QStyle::State_NoChange);
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
        bool hovered = enabled && option->state.testFlag(QStyle::State_MouseOver);

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

    // TODO: Implement animation

    qreal animationProgress = undefined ? 0.5 : (checked ? 1.0 : 0.0);

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterClipPathRollback clipRollback(painter);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QnScopedPainterOpacityRollback opacityRollback(painter);

    if (!enabled)
        painter->setOpacity(painter->opacity() * Hints::kDisabledItemOpacity);

    QSize switchSize = standalone ? Metrics::kStandaloneSwitchSize : Metrics::kButtonSwitchSize;

    QRectF rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, switchSize, option->rect);
    rect = Geometry::eroded(rect, 0.5);

    QRectF indicatorsRect = Geometry::eroded(rect, 3.5);

    /* Set clip path with excluded grip circle: */
    QRectF gripRect = Geometry::eroded(rect, 1);
    gripRect.moveLeft(gripRect.left() + (gripRect.width() - gripRect.height()) * animationProgress);
    gripRect.setWidth(gripRect.height());
    QPainterPath wholeRect;
    wholeRect.addRect(option->rect);
    QPainterPath gripCircle;
    gripCircle.addEllipse(Geometry::dilated(gripRect, 0.5));
    painter->setClipPath(wholeRect.subtracted(gripCircle));

    const double kSideTransition = 0.3;
    qreal oneMinusAnimationProgress = 1.0 - animationProgress;

    qreal baseOpacity = painter->opacity();

    /* Draw state "off", or transitional blend with it: */

    painter->setPen(QPen(frameColorOff, 0));
    painter->setBrush(fillColorOff);
    painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

    if (animationProgress < kSideTransition)
    {
        QRectF zeroRect = indicatorsRect;
        zeroRect.setLeft(indicatorsRect.right() - indicatorsRect.height());
        painter->setOpacity(baseOpacity * (kSideTransition - animationProgress) / kSideTransition);
        painter->setPen(QPen(signColorOff, 2));
        painter->drawRoundedRect(zeroRect, zeroRect.width() / 2, zeroRect.height() / 2);
    }

    /* Draw state "on", or transitional blend with it: */

    painter->setOpacity(baseOpacity * animationProgress);
    painter->setPen(QPen(frameColorOn, 0));
    painter->setBrush(fillColorOn);
    painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

    if (oneMinusAnimationProgress < kSideTransition)
    {
        int x = indicatorsRect.left() + indicatorsRect.height() / 2 + 1;
        int h = indicatorsRect.height() - 3;
        int y = rect.center().y() - h / 2;
        painter->setOpacity(baseOpacity * (kSideTransition - oneMinusAnimationProgress) / kSideTransition);
        painter->setPen(QPen(signColorOn, 2));
        painter->drawLine(x, y, x, y + h + 1);
    }

    opacityRollback.rollback();

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

    const int size = Metrics::kCheckIndicatorSize - 5;
    QRect rect = Geometry::aligned(QSize(size, size), option->rect, Qt::AlignCenter);

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

        pen.setWidthF(2);
        painter->setPen(pen);

        painter->drawPath(path);
    }
    else
    {
        painter->drawRoundedRect(aaAlignedRect, Metrics::kCheckboxCornerRadius, Metrics::kCheckboxCornerRadius);

        if (option->state.testFlag(QStyle::State_NoChange))
        {
            pen.setWidthF(2);
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

    int w = 4;
    int h = 2;
    int y = option->rect.top() + 3;
    int x = option->rect.x() + 1;
    int wstep = 4;
    int ystep = 4;

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
    QRectF crossRect = Geometry::aligned(crossSize, rect);

    QPen pen(color, 1.5, Qt::SolidLine, Qt::FlatCap);
    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawLine(crossRect.topLeft(), crossRect.bottomRight());
    painter->drawLine(crossRect.topRight(), crossRect.bottomLeft());
}

void QnNxStylePrivate::drawTextButton(
    QPainter* painter,
    const QStyleOptionButton* option,
    QPalette::ColorRole foregroundRole,
    const QWidget* widget) const
{
    Q_UNUSED(widget);

    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool hovered = option->state.testFlag(QStyle::State_MouseOver);
    const bool pressed = option->state.testFlag(QStyle::State_Sunken);

    const bool checkable = isCheckableButton(option);
    const bool hasIcon = !option->icon.isNull();
    const bool hasMenu = option->features.testFlag(QStyleOptionButton::HasMenu);

    QBrush brush = option->palette.brush(foregroundRole);
    QIcon::Mode iconMode = enabled ? QIcon::Normal : QIcon::Disabled;

    if (hovered && enabled && !pressed)
    {
        QnPaletteColor color = findColor(option->palette.color(foregroundRole));
        brush.setColor(color.lighter(2));
        iconMode = QIcon::Active;
    }

    QRect textRect(option->rect);

    if (checkable)
    {
        QStyleOption switchOption(*option); //< not QStyleOptionButton for standalone switch
        switchOption.rect.setWidth(Metrics::kStandaloneSwitchSize.width());

        if (option->direction == Qt::LeftToRight)
            textRect.setLeft(switchOption.rect.right() + Metrics::kStandardPadding + 1);
        else
            textRect.setRight(switchOption.rect.left() - Metrics::kStandardPadding - 1);

        drawSwitch(painter, &switchOption, widget);
    }

    if (hasIcon)
    {
        paintLabelIcon(
            &textRect,
            painter,
            option->icon,
            option->direction,
            Qt::AlignLeft | Qt::AlignVCenter,
            Metrics::kTextButtonIconMargin,
            iconMode);
    }

    if (hasMenu)
    {
        const auto icon = pressed
            ? qnSkin->icon(lit("text_buttons/collapse.png"))
            : qnSkin->icon(lit("text_buttons/expand.png"));

        paintLabelIcon(
            &textRect,
            painter,
            icon,
            option->direction,
            Qt::AlignRight | Qt::AlignVCenter,
            Metrics::kMenuButtonIndicatorMargin,
            iconMode);
    }

    const auto horizontalAlignment = hasMenu && !(checkable || hasIcon)
        ? Qt::AlignRight
        : Qt::AlignLeft;

    static const int kTextFlags = Qt::TextSingleLine
                                | Qt::TextHideMnemonic
                                | Qt::AlignVCenter
                                | horizontalAlignment;

    const auto text = option->fontMetrics.elidedText(
        option->text,
        Qt::ElideRight,
        textRect.width(),
        Qt::TextShowMnemonic);

    QnScopedPainterPenRollback penRollback(painter, QPen(brush.color()));
    painter->drawText(textRect, kTextFlags, text);
}

void QnNxStylePrivate::drawArrowIndicator(const QStyle* style,
    const QStyleOptionToolButton* toolbutton,
    const QRect& rect,
    QPainter* painter,
    const QWidget* widget)
{
    QStyle::PrimitiveElement pe;
    switch (toolbutton->arrowType)
    {
        case Qt::LeftArrow:
            pe = QStyle::PE_IndicatorArrowLeft;
            break;
        case Qt::RightArrow:
            pe = QStyle::PE_IndicatorArrowRight;
            break;
        case Qt::UpArrow:
            pe = QStyle::PE_IndicatorArrowUp;
            break;
        case Qt::DownArrow:
            pe = QStyle::PE_IndicatorArrowDown;
            break;
        default:
            return;
    }
    QStyleOption arrowOpt = *toolbutton;
    arrowOpt.rect = rect;
    style->drawPrimitive(pe, &arrowOpt, painter, widget);
}

void QnNxStylePrivate::drawToolButton(QPainter* painter,
    const QStyleOptionToolButton* option,
    const QWidget* widget) const
{
    // Implementation is taken from QCommonStyle except one line (see comment below).

    Q_Q(const QnNxStyle);

    QRect rect = option->rect;
    int shiftX = 0;
    int shiftY = 0;
    if (option->state & (QStyle::State_Sunken | QStyle::State_On))
    {
        shiftX = q->pixelMetric(QStyle::PM_ButtonShiftHorizontal, option, widget);
        shiftY = q->pixelMetric(QStyle::PM_ButtonShiftVertical, option, widget);
    }
    // Arrow type always overrules and is always shown
    bool hasArrow = option->features & QStyleOptionToolButton::Arrow;
    if (((!hasArrow && option->icon.isNull()) && !option->text.isEmpty())
        || option->toolButtonStyle == Qt::ToolButtonTextOnly)
    {
        int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
        if (!q->styleHint(QStyle::SH_UnderlineShortcut, option, widget))
            alignment |= Qt::TextHideMnemonic;
        rect.translate(shiftX, shiftY);
        painter->setFont(option->font);
        q->drawItemText(painter, rect, alignment, option->palette,
            option->state & QStyle::State_Enabled, option->text,
            QPalette::ButtonText);
    }
    else
    {
        QPixmap pm;
        QSize pmSize = option->iconSize;
        if (!option->icon.isNull())
        {
            QIcon::State state = option->state & QStyle::State_On ? QIcon::On : QIcon::Off;
            QIcon::Mode mode;
            if (!(option->state & QStyle::State_Enabled))
                mode = QIcon::Disabled;
            else if ((option->state & QStyle::State_MouseOver) && (option->state & QStyle::State_AutoRaise))
                mode = QIcon::Active;
            else
                mode = QIcon::Normal;

            /*
             * The only difference is here. Qt code uses rect.size() instead of iconSize here, so
             * invalid icon rect was returned on hidpi screens (e.g. 24x24 instead on 20x20).
             */
            pm = option->icon.pixmap(qt_getWindow(widget), option->iconSize, mode, state);
            pmSize = pm.size() / pm.devicePixelRatio();
        }

        if (option->toolButtonStyle != Qt::ToolButtonIconOnly)
        {
            painter->setFont(option->font);
            QRect pr = rect,
                tr = rect;
            int alignment = Qt::TextShowMnemonic;
            if (!q->styleHint(QStyle::SH_UnderlineShortcut, option, widget))
                alignment |= Qt::TextHideMnemonic;

            if (option->toolButtonStyle == Qt::ToolButtonTextUnderIcon)
            {
                pr.setHeight(pmSize.height() + 6);
                tr.adjust(0, pr.height() - 1, 0, -1);
                pr.translate(shiftX, shiftY);
                if (!hasArrow)
                {
                    q->drawItemPixmap(painter, pr, Qt::AlignCenter, pm);
                }
                else
                {
                    drawArrowIndicator(q, option, pr, painter, widget);
                }
                alignment |= Qt::AlignCenter;
            }
            else
            {
                pr.setWidth(pmSize.width() + 8);
                tr.adjust(pr.width(), 0, 0, 0);
                pr.translate(shiftX, shiftY);
                if (!hasArrow)
                {
                    q->drawItemPixmap(painter, QStyle::visualRect(option->direction, rect, pr), Qt::AlignCenter, pm);
                }
                else
                {
                    drawArrowIndicator(q, option, pr, painter, widget);
                }
                alignment |= Qt::AlignLeft | Qt::AlignVCenter;
            }
            tr.translate(shiftX, shiftY);
            q->drawItemText(painter, QStyle::visualRect(option->direction, rect, tr), alignment, option->palette,
                option->state & QStyle::State_Enabled, option->text,
                QPalette::ButtonText);
        }
        else
        {
            rect.translate(shiftX, shiftY);
            if (hasArrow)
            {
                drawArrowIndicator(q, option, rect, painter, widget);
            }
            else
            {
                q->drawItemPixmap(painter, rect, Qt::AlignCenter, pm);
            }
        }
    }
}

/* Insert horizontal separator line into QInputDialog above its button box: */
bool QnNxStylePrivate::polishInputDialog(QInputDialog* inputDialog) const
{
    NX_ASSERT(inputDialog);

    inputDialog->sizeHint(); // to ensure lazy layout is set up
    QLayout* oldLayout = inputDialog->layout();

    static const QString kTopmostLayoutName = lit("_qn_inputDialogLayout");
    if (oldLayout->objectName() == kTopmostLayoutName)
        return false;

    QDialogButtonBox* buttonBox = inputDialog->findChild<QDialogButtonBox*>(QString(), Qt::FindDirectChildrenOnly);
    if (!buttonBox || buttonBox->isHidden())
        return false;

    auto container = new QWidget(inputDialog);
    container->setLayout(oldLayout);

    auto newLayout = new QVBoxLayout(inputDialog);
    newLayout->setObjectName(kTopmostLayoutName);

    auto line = new QFrame(inputDialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    newLayout->addWidget(container);
    newLayout->addWidget(line);
    newLayout->addWidget(buttonBox);

    const int margin = Metrics::kDefaultTopLevelMargin;
    const QMargins kDefaultMargins(margin, margin, margin, margin);
    const QMargins kZeroMargins;
    buttonBox->setContentsMargins(kDefaultMargins);
    oldLayout->setContentsMargins(kDefaultMargins);
    newLayout->setContentsMargins(kZeroMargins);
    newLayout->setSpacing(0);
    newLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    return true;
}

const QWidget* QnNxStylePrivate::graphicsProxiedWidget(const QWidget* widget)
{
    while (widget && !widget->graphicsProxyWidget())
        widget = widget->parentWidget();

    return widget;
}

QGraphicsProxyWidget* QnNxStylePrivate::graphicsProxyWidget(const QWidget* widget)
{
    if (auto proxied = graphicsProxiedWidget(widget))
        return proxied->graphicsProxyWidget();

    return nullptr;
}

void QnNxStylePrivate::updateScrollAreaHover(QScrollBar* scrollBar) const
{
    QAbstractScrollArea* area = nullptr;
    for (auto parent = scrollBar->parent(); parent && !area; parent = parent->parent())
        area = qobject_cast<QAbstractScrollArea*>(parent);

    if (!area)
        return;

    const auto viewport = area->viewport();
    if (!viewport->underMouse())
        return;

    const auto globalPos = QCursor::pos(); //< relative to the primary screen
    const auto localPos = mapFromGlobal(viewport, globalPos);

    QHoverEvent hoverMove(
        QEvent::HoverMove,
        localPos, //< current position
        localPos, //< old position
        qApp->keyboardModifiers());

    qApp->notify(viewport, &hoverMove);
}
