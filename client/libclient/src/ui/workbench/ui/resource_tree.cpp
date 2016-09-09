#include "resource_tree.h"

#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/ui/buttons.h>

namespace {

static const int kShowAnimationDurationMs = 300;
static const int kHideAnimationDurationMs = 200;

}

QnWorkbenchUiResourceTree::QnWorkbenchUiResourceTree():
    widget(nullptr),
    resizerWidget(nullptr),
    ignoreResizerGeometryChanges(false),
    updateResizerGeometryLater(false),
    item(nullptr),
    backgroundItem(nullptr),
    showButton(nullptr),
    pinButton(nullptr),
    hidingProcessor(nullptr),
    showingProcessor(nullptr),
    opacityProcessor(nullptr),
    opacityAnimatorGroup(nullptr),
    xAnimator(nullptr)
{
}

void QnWorkbenchUiResourceTree::initialize(
    QGraphicsWidget* sceneWidget,
    AnimationTimer* animationTimer,
    const QnPaneSettings& settings,
    QnWorkbenchContext* context)
{
    widget = new QnResourceBrowserWidget(nullptr, context);
    widget->setAttribute(Qt::WA_TranslucentBackground);

    QPalette defaultPalette = widget->palette();
    setPaletteColor(widget, QPalette::Window, Qt::transparent);
    setPaletteColor(widget, QPalette::Base, Qt::transparent);
    setPaletteColor(widget->typeComboBox(), QPalette::Base, defaultPalette.color(QPalette::Base));

    backgroundItem = new QnControlBackgroundWidget(Qn::LeftBorder, sceneWidget);
    backgroundItem->setFrameBorders(Qt::RightEdge);

    item = new QnMaskedProxyWidget(sceneWidget);
    item->setWidget(widget);
    widget->installEventFilter(item);
    widget->setToolTipParent(item);
    item->setFocusPolicy(Qt::StrongFocus);
    item->setProperty(Qn::NoHandScrollOver, true);
    item->resize(settings.span, 0.0);

    const auto pinTreeAction = context->action(QnActions::PinTreeAction);
    pinTreeAction->setChecked(settings.state != Qn::PaneState::Unpinned);
    pinButton = NxUi::newPinButton(sceneWidget, context, pinTreeAction);
    pinButton->setFocusProxy(item);

    const auto toggleTreeAction = context->action(QnActions::ToggleTreeAction);
    toggleTreeAction->setChecked(settings.state == Qn::PaneState::Opened);
    showButton = NxUi::newShowHideButton(sceneWidget, context, toggleTreeAction);
    showButton->setFocusProxy(item);
    showButton->stackBefore(item);

    resizerWidget = new QnResizerWidget(Qt::Horizontal, sceneWidget);
    resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    resizerWidget->stackBefore(showButton);

    opacityProcessor = new HoverFocusProcessor(sceneWidget);
    opacityProcessor->addTargetItem(item);
    opacityProcessor->addTargetItem(showButton);

    hidingProcessor = new HoverFocusProcessor(sceneWidget);
    hidingProcessor->addTargetItem(item);
    hidingProcessor->addTargetItem(showButton);
    hidingProcessor->addTargetItem(resizerWidget);
    hidingProcessor->setHoverLeaveDelay(NxUi::kCloseControlsTimeoutMs);
    hidingProcessor->setFocusLeaveDelay(NxUi::kCloseControlsTimeoutMs);

    showingProcessor = new HoverFocusProcessor(sceneWidget);
    showingProcessor->addTargetItem(showButton);
    showingProcessor->setHoverEnterDelay(NxUi::kShowControlsTimeoutMs);

    xAnimator = new VariantAnimator(widget);
    xAnimator->setTimer(animationTimer);
    xAnimator->setTargetObject(item);
    xAnimator->setAccessor(new PropertyAccessor("x"));
    xAnimator->setSpeed(1.0);

    opacityAnimatorGroup = new AnimatorGroup(widget);
    opacityAnimatorGroup->setTimer(animationTimer);
    opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    opacityAnimatorGroup->addAnimator(opacityAnimator(backgroundItem)); /* Speed of 1.0 is OK here. */
    opacityAnimatorGroup->addAnimator(opacityAnimator(showButton));
    opacityAnimatorGroup->addAnimator(opacityAnimator(pinButton));
}

void QnWorkbenchUiResourceTree::setOpened(bool opened, bool animate)
{
    xAnimator->stop();
    if (opened)
        xAnimator->setEasingCurve(QEasingCurve::InOutCubic);
    else
        xAnimator->setEasingCurve(QEasingCurve::OutCubic);

    qreal width = item->size().width();
    xAnimator->setTimeLimit(opened ? kShowAnimationDurationMs : kHideAnimationDurationMs);

    qreal newX = opened ? 0.0 : - width - 1.0 /* Just in case. */;
    if (animate)
        xAnimator->animateTo(newX);
    else
        item->setX(newX);

    resizerWidget->setEnabled(opened);
}
