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

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
};

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight4Theme = {
    {QIcon::Normal, {.primary = "light4"}},
    {QIcon::Disabled, {.primary = "dark17"}}
};

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight10Theme = {
    {QIcon::Normal, {.primary = "light10"}},
    {QIcon::Disabled, {.primary = "dark17"}}
};

NX_DECLARE_COLORIZED_ICON(kArrowDownIcon, "20x20/Outline/arrow_down.svg", kIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kArrowUpIcon, "20x20/Outline/arrow_up.svg", kIconSubstitutions)

NX_DECLARE_COLORIZED_ICON(kUncheckedIcon, "12x12/Outline/checkbox_empty.svg", kLight10Theme)
NX_DECLARE_COLORIZED_ICON(kCheckedIcon, "12x12/Outline/checkbox_checked.svg", kLight4Theme)
NX_DECLARE_COLORIZED_ICON(kPartiallyCheckedIcon, "12x12/Outline/checkbox_half_checked.svg",
    kLight4Theme)

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

void StylePrivate::drawSwitch(QPainter* painter, const QStyleOption* option) const
{
    const bool pressed = option->state.testFlag(QStyle::State_Sunken);
    const bool focused = option->state.testFlag(QStyle::State_HasFocus);

    QString switchOnFillColor = "green_l";
    QString switchOnFrameColor = "transparent";

    if (focused)
        switchOnFrameColor = "light2";
    else if (pressed)
        switchOnFillColor = "green_d";

    const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSwitchOnTheme = {
        {QIcon::Normal, {.primary = "green", .secondary = "transparent", .tertiary = "dark5"}},
        {QIcon::Active,
            {.primary = switchOnFillColor, .secondary = switchOnFrameColor, .tertiary = "dark5"}},
        {QIcon::Disabled,
            {.primary = "green", .secondary = "transparent", .tertiary = "dark5", .alpha = 0.5}}};

    QString switchOffActiveColor = "light8";
    if (pressed)
        switchOffActiveColor = "light12";
    else if (focused)
        switchOffActiveColor = "light6";

    const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSwitchOffTheme = {
        {QIcon::Normal, {.primary = "light10", .secondary = "light10"}},
        {QIcon::Active, {.primary = switchOffActiveColor, .secondary = switchOffActiveColor}},
        {QIcon::Disabled, {.primary = "light10", .secondary = "light10", .alpha = 0.5}}
    };

    const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSwitchPartiallyOnTheme = {
        {QIcon::Normal, {.primary = "light4", .secondary = "light4"}},
        {QIcon::Active, {.primary = switchOffActiveColor, .secondary = switchOffActiveColor}},
        {QIcon::Disabled, {.primary = "light10", .secondary = "light10", .alpha = 0.5}}
    };

    NX_DECLARE_COLORIZED_ICON(kSwitchOn, "33x17/Solid/switch_on.svg", kSwitchOnTheme)
    NX_DECLARE_COLORIZED_ICON(kSwitchOff, "33x17/Outline/switch_off.svg", kSwitchOffTheme)
    NX_DECLARE_COLORIZED_ICON(
        kSwitchPartiallyOn, "33x17/Outline/switch_middle.svg", kSwitchPartiallyOnTheme)

    const auto rect = Geometry::aligned(Metrics::kButtonSwitchSize, option->rect, Qt::AlignCenter);

    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool hovered = enabled && option->state.testFlag(QStyle::State_MouseOver);
    const auto iconMode = enabled
        ? ((pressed || focused || hovered) ? QIcon::Active : QIcon::Normal)
        : QIcon::Disabled;

    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    if (option->state.testFlag(QStyle::State_On))
    {
        painter->drawPixmap(
            rect, qnSkin->icon(kSwitchOn).pixmap(Metrics::kButtonSwitchSize, iconMode));
    }
    else if (option->state.testFlag(QStyle::State_NoChange))
    {
        painter->drawPixmap(
            rect, qnSkin->icon(kSwitchPartiallyOn).pixmap(Metrics::kButtonSwitchSize, iconMode));
    }
    else
    {
        painter->drawPixmap(
            rect, qnSkin->icon(kSwitchOff).pixmap(Metrics::kButtonSwitchSize, iconMode));
    }
    painter->restore();
}

void StylePrivate::drawCheckBox(
    QPainter* painter, const QStyleOption* option, const QWidget* widget) const
{
    Q_UNUSED(widget)

    const auto iconMode = option->state.testFlag(QStyle::State_Enabled)
        ? QIcon::Normal
        : QIcon::Disabled;

    const auto iconSize = QSize(12, 12);
    const auto rect = Geometry::aligned(iconSize, option->rect, Qt::AlignCenter);

    if (option->state.testFlag(QStyle::State_On))
        painter->drawPixmap(rect, qnSkin->icon(kCheckedIcon).pixmap(iconSize, iconMode));
    else if (option->state.testFlag(QStyle::State_NoChange))
        painter->drawPixmap(rect, qnSkin->icon(kPartiallyCheckedIcon).pixmap(iconSize, iconMode));
    else
        painter->drawPixmap(rect, qnSkin->icon(kUncheckedIcon).pixmap(iconSize, iconMode));
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
        switchOption.rect.setWidth(Metrics::kButtonSwitchSize.width());

        if (option->direction == Qt::LeftToRight)
            textRect.setLeft(switchOption.rect.right() + Metrics::kStandardPadding + 1);
        else
            textRect.setRight(switchOption.rect.left() - Metrics::kStandardPadding - 1);

        drawSwitch(painter, &switchOption);
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
        const auto icon = pressed ? qnSkin->icon(kArrowUpIcon) : qnSkin->icon(kArrowDownIcon);

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
