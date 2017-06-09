#include "buttons.h"

#include <ui/actions/action.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include <nx/fusion/model_functions.h>

namespace {

QString aliasFromAction(QAction *action)
{
    static const auto kUndefinedAlias = lit("undefined");
    const auto ourAction = dynamic_cast<QnAction *>(action);
    if (!ourAction)
        return kUndefinedAlias;
    return QnLexical::serialized(ourAction->id());
}

} //namespace

namespace NxUi {

QnImageButtonWidget* newActionButton(QGraphicsItem *parent, QnWorkbenchContext* context,
    QAction* action, int helpTopicId)
{
    QnImageButtonWidget* button = new QnImageButtonWidget(parent);
    context->statisticsModule()->registerButton(aliasFromAction(action), button);

    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton);
    button->setDefaultAction(action);
    button->setFixedSize(QnSkin::maximumSize(action->icon()));

    if (helpTopicId != Qn::Empty_Help)
        setHelpTopic(button, helpTopicId);

    return button;
}

template<class T>
T* newCustomShowHideButton(QGraphicsItem* parent, QnWorkbenchContext* context, QAction* action)
{
    auto button = new T(parent);
    context->statisticsModule()->registerButton(aliasFromAction(action), button);

    if (action)
        button->setDefaultAction(action);
    else
        button->setCheckable(true);

    button->setIcon(qnSkin->icon("panel/slide_right.png", "panel/slide_left.png"));
    button->setFixedSize(QnSkin::maximumSize(button->icon()));

    button->setProperty(Qn::NoHandScrollOver, true);
    button->setProperty(Qn::BlockMotionSelection, true);

    setHelpTopic(button, Qn::MainWindow_Pin_Help);
    return button;
}

QnImageButtonWidget* newShowHideButton(QGraphicsItem* parent, QnWorkbenchContext* context,
    QAction* action)
{
    return newCustomShowHideButton<QnImageButtonWidget>(parent, context, action);
}

QnBlinkingImageButtonWidget* newBlinkingShowHideButton(QGraphicsItem* parent, QnWorkbenchContext* context, QAction* action)
{
    return newCustomShowHideButton<QnBlinkingImageButtonWidget>(parent, context, action);
}

QnImageButtonWidget* newPinButton(QGraphicsItem* parent, QnWorkbenchContext* context,
    QAction* action, bool smallIcon)
{
    QnImageButtonWidget* button = new QnImageButtonWidget(parent);
    context->statisticsModule()->registerButton(aliasFromAction(action), button);

    if (action)
        button->setDefaultAction(action);
    else
        button->setCheckable(true);

    if (smallIcon)
        button->setIcon(qnSkin->icon("panel/pin_small.png", "panel/unpin_small.png"));
    else
        button->setIcon(qnSkin->icon("panel/pin.png", "panel/unpin.png"));

    button->setFixedSize(QnSkin::maximumSize(button->icon()));

    setHelpTopic(button, Qn::MainWindow_Pin_Help);
    return button;
}

} //namespace NxUi
