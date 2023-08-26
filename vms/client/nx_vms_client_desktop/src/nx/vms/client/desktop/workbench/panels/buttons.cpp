// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buttons.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using namespace ui;

namespace {

QString alias(action::Action* action)
{
    return QnLexical::serialized(action->id());
}

// TODO: @pprivalov Remove this old fashioned color substitutions when figma plugin is ready
static const QColor kBasePrimaryColor = "#6F8694"; // ~light16
static const QColor kBackgroundColor = "#263137"; // dark8

const core::SvgIconColorer::IconSubstitutions kSlideIconSubstitutions = {
    { QnIcon::Normal, {
        { kBasePrimaryColor, "light16"},
        { kBackgroundColor, "dark8"}
    }},
    { QnIcon::Selected, { // Pressed
        { kBasePrimaryColor, "light15"},
        { kBackgroundColor, "dark7" },
    }},
    { QnIcon::Active, { // Hovered
        { kBasePrimaryColor, "light17"},
        { kBackgroundColor, "dark9" },
    }},
};

} //namespace

QnImageButtonWidget* newActionButton(
    QGraphicsItem *parent,
    QnWorkbenchContext* context,
    action::IDType actionId,
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
    QnWorkbenchContext* context,
    action::IDType actionId)
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

    button->setIcon(qnSkin->icon(
        "panel/slide_right_12x32.svg", kSlideIconSubstitutions, "panel/slide_left_12x32.svg"));
    button->setFixedSize(core::Skin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, HelpTopic::Id::MainWindow_Pin);
    return button;
}

QnImageButtonWidget* newShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    action::IDType actionId)
{
    return newCustomShowHideButton<QnImageButtonWidget>(parent, context, actionId);
}

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    action::IDType actionId)
{
    return newCustomShowHideButton<QnBlinkingImageButtonWidget>(parent, context, actionId);
}

QnImageButtonWidget* newPinTimelineButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    action::IDType actionId)
{
    NX_ASSERT(context);
    const auto action = context->menu()->action(actionId);

    auto button = new QnImageButtonWidget(parent);
    statisticsModule()->controls()->registerButton(
        alias(action),
        button);

    if (action)
        button->setDefaultAction(action);

    button->setIcon(qnSkin->icon("panel/pin_12x32.svg", kSlideIconSubstitutions));
    button->setFixedSize(core::Skin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, HelpTopic::Id::MainWindow_Pin);
    return button;
}

} //namespace nx::vms::client::desktop
