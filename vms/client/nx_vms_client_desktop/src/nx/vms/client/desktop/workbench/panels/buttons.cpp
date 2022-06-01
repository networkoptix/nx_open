// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buttons.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using namespace ui;

namespace {

QString alias(action::Action* action)
{
    return QnLexical::serialized(action->id());
}

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
    button->setFixedSize(Skin::maximumSize(action->icon()));

    if (helpTopicId != Qn::Empty_Help)
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

    button->setIcon(qnSkin->icon("panel/slide_right.png", "panel/slide_left.png"));
    button->setFixedSize(Skin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, Qn::MainWindow_Pin_Help);
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

    button->setIcon(qnSkin->icon("panel/slide_pin_left.png"));
    button->setFixedSize(Skin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, Qn::MainWindow_Pin_Help);
    return button;
}

QnImageButtonWidget* newPinButton(
    QGraphicsItem* parent,
    QnWorkbenchContext* context,
    action::IDType actionId,
    bool smallIcon)
{
    NX_ASSERT(context);
    const auto action = context->menu()->action(actionId);

    QnImageButtonWidget* button = new QnImageButtonWidget(parent);
    statisticsModule()->controls()->registerButton(
        alias(action),
        button);

    if (action)
        button->setDefaultAction(action);
    else
        button->setCheckable(true);

    if (smallIcon)
        button->setIcon(qnSkin->icon("panel/pin_small.png", "panel/unpin_small.png"));
    else
        button->setIcon(qnSkin->icon("panel/pin.png", "panel/unpin.png"));

    button->setFixedSize(Skin::maximumSize(button->icon()));

    setHelpTopic(button, Qn::MainWindow_Pin_Help);
    return button;
}

} //namespace nx::vms::client::desktop
