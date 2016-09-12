#include "timeline_workbench_panel.h"

#include <client/client_runtime_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

namespace {

static const int kDefaultThumbnailsHeight = 48;
static const int kVideoWallTimelineAutoHideTimeoutMs = 10000;

static const int kShowAnimationDurationMs = 240;
static const int kHideAnimationDurationMs = 160;

static const int kShowTimelineTimeoutMs = 100;
static const int kCloseTimelineTimeoutMs = 250;

}

namespace NxUi {

NxUi::TimelineWorkbenchPanel::TimelineWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    pinned(false),
    item(new QnNavigationItem(parentWidget)),
    resizerWidget(new QnResizerWidget(Qt::Vertical, parentWidget)),
    ignoreResizerGeometryChanges(false),
    updateResizerGeometryLater(false),
    zoomingIn(false),
    zoomingOut(false),
    zoomButtonsWidget(nullptr),
    opacityProcessor(nullptr),
    yAnimator(nullptr),
    showButton(nullptr),
    showWidget(new GraphicsWidget(parentWidget)),
    showingProcessor(new HoverFocusProcessor(parentWidget)),
    hidingProcessor(new HoverFocusProcessor(parentWidget)),
    opacityAnimatorGroup(nullptr),
    autoHideTimer(nullptr),
    lastThumbnailsHeight(kDefaultThumbnailsHeight),

    m_visible(false),
    m_opened(false)
{
    resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    resizerWidget->setZValue(ResizerItemZOrder);

    item->setProperty(Qn::NoHandScrollOver, true);
    item->setZValue(ContentItemZOrder);

    const auto toggleSliderAction = action(QnActions::ToggleSliderAction);
    toggleSliderAction->setChecked(settings.state == Qn::PaneState::Opened);
    showButton = NxUi::newShowHideButton(parentWidget, context(), toggleSliderAction);
    {
        QTransform transform;
        transform.rotate(-90);
        showButton->setTransform(transform);
    }
    showButton->setFocusProxy(item);
    showButton->setZValue(ControlItemZOrder);

    showWidget->setProperty(Qn::NoHandScrollOver, true);
    showWidget->setFlag(QGraphicsItem::ItemHasNoContents, true);
    showWidget->setVisible(false);
    showWidget->setZValue(ControlItemZOrder);

    showingProcessor->addTargetItem(showButton);
    showingProcessor->addTargetItem(showWidget);
    showingProcessor->addTargetItem(resizerWidget);
    showingProcessor->setHoverEnterDelay(kShowTimelineTimeoutMs);

    hidingProcessor->addTargetItem(item);
    hidingProcessor->addTargetItem(showButton);
    hidingProcessor->addTargetItem(resizerWidget);
    hidingProcessor->addTargetItem(showWidget);
    hidingProcessor->setHoverLeaveDelay(kCloseTimelineTimeoutMs);
    hidingProcessor->setFocusLeaveDelay(kCloseTimelineTimeoutMs);

    if (qnRuntime->isVideoWallMode())
    {
        showButton->setVisible(false);
        autoHideTimer = new QTimer(this);
        connect(autoHideTimer, &QTimer::timeout, this,
            [this]
            {
                setVisible(false, true);
            });
    }
}

bool TimelineWorkbenchPanel::isPinned() const
{
    return pinned;
}

bool TimelineWorkbenchPanel::isOpened() const
{
    return m_opened;
}

void TimelineWorkbenchPanel::setOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);

    m_opened = opened;

    yAnimator->stop();
    if (opened)
        yAnimator->setEasingCurve(QEasingCurve::InOutCubic);
    else
        yAnimator->setEasingCurve(QEasingCurve::OutCubic);

    yAnimator->setTimeLimit(opened ? kShowAnimationDurationMs : kHideAnimationDurationMs);

    auto parentWidgetRect = m_parentWidget->rect();
    qreal newY = parentWidgetRect.bottom()
        + (opened ? -item->size().height() : 64.0 /* So that tooltips are not opened. */);
    if (animate)
    {
        /* Skip off-screen part. */
        if (opened && item->y() > parentWidgetRect.bottom())
        {
            QSignalBlocker blocker(item);
            item->setY(parentWidgetRect.bottom());
        }
        yAnimator->animateTo(newY);
    }
    else
    {
        item->setY(newY);
    }
    resizerWidget->setEnabled(opened);

    emit openedChanged(opened, animate);
}

bool TimelineWorkbenchPanel::isVisible() const
{
    return m_visible;
}

void TimelineWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;

    item->setEnabled(visible); /* So that it doesn't handle mouse events while disappearing. */
    updateOpacity(animate);

    if (qnRuntime->isVideoWallMode())
    {
        if (visible)
            autoHideTimer->start(kVideoWallTimelineAutoHideTimeoutMs);
        else
            autoHideTimer->stop();
    }

    if (changed)
        emit visibleChanged(visible, animate);
}

qreal TimelineWorkbenchPanel::opacity() const
{
    return opacityAnimator(item)->targetValue().toDouble();
}

void TimelineWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity);
        opacityAnimator(showButton)->setTargetValue(opacity);
        opacityAnimatorGroup->start();
    }
    else
    {
        opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        showButton->setOpacity(opacity);
    }

    resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

void TimelineWorkbenchPanel::updateOpacity(bool animate)
{
    base_type::updateOpacity(animate);

    bool isButtonOpaque = m_visible && opacityProcessor->isHovered();
    const qreal buttonsOpacity = isButtonOpaque ? NxUi::kOpaque : NxUi::kHidden;

    ensureAnimationAllowed(&animate);
    if (animate)
        opacityAnimator(zoomButtonsWidget)->animateTo(buttonsOpacity);
    else
        zoomButtonsWidget->setOpacity(buttonsOpacity);
}

bool TimelineWorkbenchPanel::isHovered() const
{
    return opacityProcessor->isHovered();
}

} //namespace NxUi