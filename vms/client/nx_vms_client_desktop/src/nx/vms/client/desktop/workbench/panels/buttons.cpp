// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buttons.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>

namespace nx::vms::client::desktop {

namespace {

QString alias(menu::Action* action)
{
    return QnLexical::serialized(action->id());
}

const core::SvgIconColorer::ThemeSubstitutions kSlideIconSubstitutions = {
    {QnIcon::Normal,
        {.primary = "light16", .secondary = "dark8"}},
    {QnIcon::Selected, // Pressed.
        {.primary = "light15", .secondary = "dark7"}},
    {QnIcon::Active, //< Hovered.
        {.primary = "light17", .secondary = "dark9"}}};

NX_DECLARE_COLORIZED_ICON(kPinIcon, "12x32/Solid/panel_pin.svg", kSlideIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSlideIcon,
    "12x32/Solid/panel_right.svg", kSlideIconSubstitutions,
    "12x32/Solid/panel_left.svg", kSlideIconSubstitutions)
} //namespace

QnImageButtonWidget* newActionButton(
    QGraphicsItem *parent,
    WindowContext* context,
    menu::IDType actionId,
    int helpTopicId)
{
    NX_ASSERT(context);
    const auto action = context->menu()->action(actionId);

    QnImageButtonWidget* button = new QnImageButtonWidget(parent);
    statisticsModule()->controls()->registerButton(
        alias(action),
        button);

    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton);
    button->setDefaultAction(action);
    button->setFixedSize(core::Skin::maximumSize(action->icon()));

    if (helpTopicId != HelpTopic::Id::Empty)
        setHelpTopic(button, helpTopicId);

    return button;
}

template<class T>
T* newCustomShowHideButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId)
{
    NX_ASSERT(context);
    const auto action = context->menu()->action(actionId);

    auto button = new T(parent);
    statisticsModule()->controls()->registerButton(
        alias(action),
        button);

    if (action)
        button->setDefaultAction(action);
    else
        button->setCheckable(true);

    button->setIcon(qnSkin->icon(kSlideIcon));
    button->setFixedSize(core::Skin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, HelpTopic::Id::MainWindow_Pin);
    return button;
}

QnImageButtonWidget* newShowHideButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId)
{
    return newCustomShowHideButton<QnImageButtonWidget>(parent, context, actionId);
}

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId)
{
    return newCustomShowHideButton<QnBlinkingImageButtonWidget>(parent, context, actionId);
}

QnImageButtonWidget* newPinTimelineButton(
    QGraphicsItem* parent,
    WindowContext* context,
    menu::IDType actionId)
{
    NX_ASSERT(context);
    const auto action = context->menu()->action(actionId);

    auto button = new QnImageButtonWidget(parent);
    statisticsModule()->controls()->registerButton(
        alias(action),
        button);

    if (action)
        button->setDefaultAction(action);

    button->setIcon(qnSkin->icon(kPinIcon));
    button->setFixedSize(core::Skin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, HelpTopic::Id::MainWindow_Pin);
    return button;
}

} //namespace nx::vms::client::desktop
