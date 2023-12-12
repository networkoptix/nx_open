// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "style_private.h"

#include <limits>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QScrollBar>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/slide_switch.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace nx::style;
using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

namespace {

static const QColor kLight10Color = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light10"}}},
    {QIcon::Active, {{kLight10Color, "light11"}}},
};

static const QColor kTextButtonBackgroundColor = "#FFFFFF";

void paintLabelIcon(QRect* labelRect,
    QPainter* painter,
    const QIcon& icon,
    Qt::LayoutDirection direction,
    Qt::Alignment alignment,
    int padding,
    QIcon::Mode mode = QIcon::Normal,
    QIcon::State state = QIcon::Off)
{
    QSize iconSize = core::Skin::maximumSize(icon, mode, state);
    QRect iconRect = QStyle::alignedRect(direction, alignment, iconSize, *labelRect);

    icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);

    if (labelRect->left() == iconRect.left())
        labelRect->setLeft(iconRect.right() + padding + 1);
    else
        labelRect->setRight(iconRect.left() - padding - 1);
}

} // namespace

StylePrivate::StylePrivate():
    StyleBasePrivate(),
    idleAnimator(new AbstractWidgetAnimation())
{
}

StylePrivate::~StylePrivate()
{
}

QColor StylePrivate::checkBoxColor(const QStyleOption* option, bool radio) const
{
    // Use default color for disabled checkboxes.
    if (!option->state.testFlag(QStyle::State_Enabled))
        return option->palette.color(QPalette::Text);

    // This function is unreliable and returns unexpected result over a wide range of input data.
    // For item view checkboxes color is taken as is from the passed style option. Thus item
    // delegate is fully responsible for the checkbox appearance.
    if (option->type == QStyleOption::SO_ViewItem)
        return option->palette.color(QPalette::Text);

    const bool checked = option->state.testFlag(QStyle::State_On)
        || option->state.testFlag(QStyle::State_NoChange);
    NX_ASSERT(checked || option->state.testFlag(QStyle::State_Off));
    const bool hovered = option->state.testFlag(QStyle::State_MouseOver);

    const QColor mainColor = option->palette.color(QPalette::Active, QPalette::Text);
    const QColor secondaryColor = option->palette.color(QPalette::Inactive, QPalette::Text);
    const bool customized = (mainColor != secondaryColor);

    if (customized)
    {
        const QColor& result = checked ? mainColor : secondaryColor;
        return hovered ? result.lighter(125) : result;
    }

    if (checked)
    {
        return hovered && !radio
            ? core::colorTheme()->lighter(mainColor, 3)
            : mainColor;
    }

    return hovered
        ? core::colorTheme()->darker(mainColor, 4)
        : core::colorTheme()->darker(mainColor, 6);
}

bool StylePrivate::isCheckableButton(const QStyleOption* option)
{
    if (qstyleoption_cast<const QStyleOptionButton*>(option))
    {
        if (option->state.testFlag(QStyle::State_On) || option->state.testFlag(QStyle::State_Off)
            || option->state.testFlag(QStyle::State_NoChange))
        {
            return true;
        }

        const QAbstractButton* buttonWidget =
            qobject_cast<const QAbstractButton*>(option->styleObject);
        if (buttonWidget && buttonWidget->isCheckable())
            return true;
    }

    return false;
}

bool StylePrivate::isTextButton(const QStyleOption* option)
{
    if (auto button = qstyleoption_cast<const QStyleOptionButton*>(option))
        return button->features.testFlag(QStyleOptionButton::Flat);

    return false;
}

void StylePrivate::drawSwitch(
    QPainter* painter, const QStyleOption* option, const QWidget* widget) const
{
    Q_UNUSED(widget);

    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool undefined = option->state.testFlag(QStyle::State_NoChange);
    const bool standalone = !qstyleoption_cast<const QStyleOptionButton*>(option)
        || qobject_cast<const SlideSwitch*>(widget);

    QColor fillColorOn; //< On: background
    QColor fillColorOff; //< Off: background
    QColor frameColorOn; //< On: frame
    QColor frameColorOff; //< Off: frame
    QColor signColorOn; //< On: "1" indicator
    QColor signColorOff; //< Off: "0" indicator

    if (standalone)
    {
        bool hovered = enabled && option->state.testFlag(QStyle::State_MouseOver);

        fillColorOff = hovered ? core::colorTheme()->color("dark14") : core::colorTheme()->color("dark13");
        fillColorOn =
            hovered ? core::colorTheme()->color("green_l1") : core::colorTheme()->color("green_core");
        frameColorOff = fillColorOff;
        frameColorOn = fillColorOn;
        signColorOff = core::colorTheme()->color("dark17");
        signColorOn = hovered ? core::colorTheme()->color("green_l3") : core::colorTheme()->color("green_l2");
    }
    else
    {
        fillColorOff = core::colorTheme()->color("dark8");
        fillColorOn = core::colorTheme()->color("green_core");
        frameColorOff = core::colorTheme()->color("dark7");
        frameColorOn = fillColorOn;
        signColorOff = core::colorTheme()->color("dark10");
        signColorOn = core::colorTheme()->color("green_l2");
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
    gripRect.moveLeft(
        gripRect.left() + (gripRect.width() - gripRect.height()) * animationProgress);
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
        painter->setOpacity(
            baseOpacity * (kSideTransition - oneMinusAnimationProgress) / kSideTransition);
        painter->setPen(QPen(signColorOn, 2));
        painter->drawLine(x, y, x, y + h + 1);
    }

    opacityRollback.rollback();

    if (standalone && option->state.testFlag(QStyle::State_HasFocus))
    {
        Q_Q(const Style);
        QStyleOptionFocusRect focusOption;
        focusOption.QStyleOption::operator=(*option);
        q->proxy()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOption, painter, widget);
    }
}

void StylePrivate::drawCheckBox(
    QPainter* painter, const QStyleOption* option, const QWidget* widget) const
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
        // Draw checkbox frame with cutout at top right corner as path, counterclockwise. Approach
        // with clipping region didn't work well when checkbox primitive was drawn by item view
        // delegate due Qt bug.

        RectCoordinates aaAlignedRc(aaAlignedRect);

        QPainterPath framePath;
        framePath.moveTo(aaAlignedRc.x(0.8), aaAlignedRc.y(0));

        QRectF cornerArcRect(
            QPointF(), QSize(Metrics::kCheckboxCornerRadius, Metrics::kCheckboxCornerRadius));

        // Top line segment and top left corner arc.
        cornerArcRect.moveTopLeft(aaAlignedRect.topLeft());
        framePath.lineTo(cornerArcRect.topRight());
        framePath.arcTo(cornerArcRect, 90, 90);

        // Left line segment and bottom left corner arc.
        cornerArcRect.moveBottomLeft(aaAlignedRect.bottomLeft());
        framePath.lineTo(cornerArcRect.topLeft());
        framePath.arcTo(cornerArcRect, 180, 90);

        // Bottom line segment and bottom right corner arc.
        cornerArcRect.moveBottomRight(aaAlignedRect.bottomRight());
        framePath.lineTo(cornerArcRect.bottomLeft());
        framePath.arcTo(cornerArcRect, 270, 90);

        // Finally, right line segment.
        framePath.lineTo(aaAlignedRc.x(1), aaAlignedRc.y(0.5));
        painter->drawPath(framePath);

        // Draw check mark.

        QPainterPath checkmarkPath;
        checkmarkPath.moveTo(rc.x(0.25), rc.y(0.38));
        checkmarkPath.lineTo(rc.x(0.55), rc.y(0.70));
        checkmarkPath.lineTo(rc.x(1.12), rc.y(0.13));

        pen.setWidthF(2);
        painter->setPen(pen);

        painter->drawPath(checkmarkPath);
    }
    else
    {
        painter->drawRoundedRect(
            aaAlignedRect, Metrics::kCheckboxCornerRadius, Metrics::kCheckboxCornerRadius);

        if (option->state.testFlag(QStyle::State_NoChange))
        {
            pen.setWidthF(2);
            painter->setPen(pen);
            painter->drawLine(QPointF(rc.x(0.2), rc.y(0.5)), QPointF(rc.x(0.9), rc.y(0.5)));
        }
    }
}

void StylePrivate::drawRadioButton(
    QPainter* painter, const QStyleOption* option, const QWidget* widget) const
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

void StylePrivate::drawSortIndicator(
    QPainter* painter, const QStyleOption* option, const QWidget* widget) const
{
    Q_UNUSED(widget)

    bool down = true;
    if (const QStyleOptionHeader* header = qstyleoption_cast<const QStyleOptionHeader*>(option))
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

void StylePrivate::drawCross(QPainter* painter, const QRect& rect, const QColor& color) const
{
    const QSizeF crossSize(Metrics::kCrossSize, Metrics::kCrossSize);
    QRectF crossRect = Geometry::aligned(crossSize, rect);

    QPen pen(color, 1.5, Qt::SolidLine, Qt::FlatCap);
    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawLine(crossRect.topLeft(), crossRect.bottomRight());
    painter->drawLine(crossRect.topRight(), crossRect.bottomLeft());
}

void StylePrivate::drawTextButton(QPainter* painter,
    const QStyleOptionButton* option,
    QPalette::ColorRole foregroundRole,
    const QWidget* widget) const
{
    Q_UNUSED(widget);

    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool hovered = option->state.testFlag(QStyle::State_MouseOver);
    const bool pressed = option->state.testFlag(QStyle::State_Sunken);

    const bool checkable = isCheckableButton(option);
    const bool hasBackground = hasTextButtonBackgroundStyle(widget);
    const bool hasIcon = !option->icon.isNull();
    const bool hasMenu = option->features.testFlag(QStyleOptionButton::HasMenu);

    QIcon::Mode iconMode = enabled ? QIcon::Normal : QIcon::Disabled;
    QColor textColor;

    if (hasBackground)
    {
        QColor backgroundColor = kTextButtonBackgroundColor;

        if (pressed)
        {
            backgroundColor.setAlphaF(0.05);
            textColor = core::colorTheme()->color("light7");
        }
        else if (hovered)
        {
            backgroundColor.setAlphaF(0.15);
            textColor = core::colorTheme()->color("light1");
        }
        else if (enabled)
        {
            backgroundColor.setAlphaF(0.10);
            textColor = core::colorTheme()->color("light4");
        }
        else
        {
            backgroundColor.setAlphaF(0.03);
            textColor = core::colorTheme()->color("light4");
            textColor.setAlphaF(0.3); //< #spanasenko Is it the right way?
        }

        QnScopedPainterPenRollback pen(painter, Qt::NoPen);
        QnScopedPainterBrushRollback brush(painter, backgroundColor);
        painter->drawRoundedRect(option->rect, 4, 4);
    }
    else
    {
        QBrush brush = option->palette.brush(foregroundRole);
        if (hovered && enabled && !pressed)
        {
            brush.setColor(core::colorTheme()->lighter(option->palette.color(foregroundRole), 2));
            iconMode = QIcon::Active;
        }
        textColor = brush.color();
    }

    QRect textRect(option->rect);

    if (hasBackground)
    {
        textRect.adjust(
            Metrics::kTextButtonBackgroundMargin, 0, -Metrics::kTextButtonBackgroundMargin, 0);
    }

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
        paintLabelIcon(&textRect,
            painter,
            option->icon,
            option->direction,
            Qt::AlignLeft | Qt::AlignVCenter,
            textButtonIconSpacing(widget),
            iconMode);
    }

    if (hasMenu)
    {
        const auto icon = pressed
            ? qnSkin->icon("text_buttons/arrow_up_20.svg", kIconSubstitutions)
            : qnSkin->icon("text_buttons/arrow_down_20.svg", kIconSubstitutions);

        paintLabelIcon(&textRect,
            painter,
            icon,
            option->direction,
            Qt::AlignRight | Qt::AlignVCenter,
            Metrics::kMenuButtonIndicatorMargin,
            iconMode);
    }

    const auto horizontalAlignment =
        hasMenu && !(checkable || hasIcon) ? Qt::AlignRight : Qt::AlignLeft;

    static const int kTextFlags =
        Qt::TextSingleLine | Qt::TextHideMnemonic | Qt::AlignVCenter | horizontalAlignment;

    const auto text = option->fontMetrics.elidedText(
        option->text, Qt::ElideRight, textRect.width(), Qt::TextShowMnemonic);

    QnScopedPainterPenRollback penRollback(painter, QPen(textColor));
    painter->drawText(textRect, kTextFlags, text);
}

/* Insert horizontal separator line into QInputDialog above its button box: */
bool StylePrivate::polishInputDialog(QInputDialog* inputDialog) const
{
    NX_ASSERT(inputDialog);

    inputDialog->sizeHint(); // to ensure lazy layout is set up
    QLayout* oldLayout = inputDialog->layout();

    static const QString kTopmostLayoutName = "_qn_inputDialogLayout";
    if (oldLayout->objectName() == kTopmostLayoutName)
        return false;

    QDialogButtonBox* buttonBox =
        inputDialog->findChild<QDialogButtonBox*>(QString(), Qt::FindDirectChildrenOnly);
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

void StylePrivate::updateScrollAreaHover(QScrollBar* scrollBar) const
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
    const auto localPos =
        nx::vms::client::desktop::WidgetUtils::mapFromGlobal(viewport, globalPos);

    QHoverEvent hoverMove(QEvent::HoverMove,
        localPos, //< current position
        localPos, //< old position
        qApp->keyboardModifiers());

    qApp->notify(viewport, &hoverMove);
}

} // namespace nx::vms::client::desktop
