#include "timeline_workbench_panel.h"

#include <client/client_runtime_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/event_processors.h>

namespace {

static const int kDefaultThumbnailsHeight = 48;
static const int kVideoWallTimelineAutoHideTimeoutMs = 10000;

static const int kShowAnimationDurationMs = 240;
static const int kHideAnimationDurationMs = 160;

static const int kShowTimelineTimeoutMs = 100;
static const int kCloseTimelineTimeoutMs = 250;

}

namespace NxUi {

TimelineWorkbenchPanel::TimelineWorkbenchPanel(
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
    zoomButtonsWidget(new GraphicsWidget(parentWidget)),
    yAnimator(nullptr),
    showButton(nullptr),
    showWidget(new GraphicsWidget(parentWidget)),
    autoHideTimer(nullptr),
    lastThumbnailsHeight(kDefaultThumbnailsHeight),

    m_visible(false),
    m_opened(false),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this))
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

    m_showingProcessor->addTargetItem(showButton);
    m_showingProcessor->addTargetItem(showWidget);
    m_showingProcessor->addTargetItem(resizerWidget);
    m_showingProcessor->setHoverEnterDelay(kShowTimelineTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]
        {
            if (!isPinned() && !isOpened())
            {
                //TODO: #GDM #high process handlers
                setOpened(true);

                /* So that the click that may follow won't hide it. */
                setShowButtonUsed(false);
                QTimer::singleShot(NxUi::kButtonInactivityTimeoutMs, this,
                    [this]
                    {
                        setShowButtonUsed(true);
                    });
            }

            m_hidingProcessor->forceHoverEnter();
        });

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(showButton);
    m_hidingProcessor->addTargetItem(resizerWidget);
    m_hidingProcessor->addTargetItem(showWidget);
    m_hidingProcessor->setHoverLeaveDelay(kCloseTimelineTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(kCloseTimelineTimeoutMs);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            /* Do not auto-hide slider if we have opened context menu. */
            if (!isPinned() && isOpened() && !menu()->isMenuVisible())
                setOpened(false); //TODO: #GDM #high process handlers
        });
    connect(menu(), &QnActionManager::menuAboutToHide, m_hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);

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

    item->timeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    item->timeSlider()->toolTipItem()->setZValue(NxUi::TooltipItemZOrder);
    item->speedSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    item->speedSlider()->toolTipItem()->setZValue(NxUi::TooltipItemZOrder);
    item->volumeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    item->volumeSlider()->toolTipItem()->setZValue(NxUi::TooltipItemZOrder);

    auto sliderZoomOutButton = new QnImageButtonWidget();
    sliderZoomOutButton->setIcon(qnSkin->icon("slider/buttons/zoom_out.png"));
    sliderZoomOutButton->setPreferredSize(19, 16);
    context()->statisticsModule()->registerButton(lit("slider_zoom_out"), sliderZoomOutButton);

    auto sliderZoomInButton = new QnImageButtonWidget();
    sliderZoomInButton->setIcon(qnSkin->icon("slider/buttons/zoom_in.png"));
    sliderZoomInButton->setPreferredSize(19, 16);
    context()->statisticsModule()->registerButton(lit("slider_zoom_in"), sliderZoomInButton);

    QGraphicsLinearLayout *sliderZoomButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    sliderZoomButtonsLayout->setSpacing(0.0);
    sliderZoomButtonsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    sliderZoomButtonsLayout->addItem(sliderZoomOutButton);
    sliderZoomButtonsLayout->addItem(sliderZoomInButton);

    zoomButtonsWidget->setLayout(sliderZoomButtonsLayout);
    zoomButtonsWidget->setOpacity(0.0);

    zoomButtonsWidget->setVisible(navigator()->hasArchive());
    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged, this,
        [this]()
        {
            zoomButtonsWidget->setVisible(navigator()->hasArchive());
        });

    enum { kSliderLeaveTimeout = 100 };
    m_opacityProcessor->setHoverLeaveDelay(kSliderLeaveTimeout);
    item->timeSlider()->bookmarksViewer()->setHoverProcessor(m_opacityProcessor);
    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(item->timeSlider()->toolTipItem());
    m_opacityProcessor->addTargetItem(showButton);
    m_opacityProcessor->addTargetItem(resizerWidget);
    m_opacityProcessor->addTargetItem(showWidget);
    m_opacityProcessor->addTargetItem(zoomButtonsWidget);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    yAnimator = new VariantAnimator(this);
    yAnimator->setTimer(animationTimer());
    yAnimator->setTargetObject(item);
    yAnimator->setAccessor(new PropertyAccessor("y"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(showButton));

    auto sliderWheelEater = new QnSingleEventEater(this);
    sliderWheelEater->setEventType(QEvent::GraphicsSceneWheel);
    resizerWidget->installEventFilter(sliderWheelEater);

    auto sliderWheelSignalizer = new QnSingleEventSignalizer(this);
    sliderWheelSignalizer->setEventType(QEvent::GraphicsSceneWheel);
    resizerWidget->installEventFilter(sliderWheelSignalizer);

    connect(sliderZoomInButton, &QnImageButtonWidget::pressed, this,
        [this]
        {
            zoomingIn = true;
        });

    connect(sliderZoomInButton, &QnImageButtonWidget::released, this,
        [this]
        {
            zoomingIn = false;
            item->timeSlider()->hurryKineticAnimations();
        });

    connect(sliderZoomOutButton, &QnImageButtonWidget::pressed, this,
        [this]
        {
            zoomingOut = true;
        });

    connect(sliderZoomOutButton, &QnImageButtonWidget::released, this,
        [this]
        {
            zoomingOut = false;
            item->timeSlider()->hurryKineticAnimations();
        });

    connect(sliderWheelSignalizer, &QnAbstractEventSignalizer::activated, this,
        &TimelineWorkbenchPanel::at_sliderResizerWidget_wheelEvent);

    /* Create a shadow: */
    new QnEdgeShadowWidget(item, Qt::TopEdge, NxUi::kShadowThickness);
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
        m_opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity);
        opacityAnimator(showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        showButton->setOpacity(opacity);
    }

    resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

void TimelineWorkbenchPanel::updateOpacity(bool animate)
{
    base_type::updateOpacity(animate);

    bool isButtonOpaque = m_visible && m_opacityProcessor->isHovered();
    const qreal buttonsOpacity = isButtonOpaque ? NxUi::kOpaque : NxUi::kHidden;

    ensureAnimationAllowed(&animate);
    if (animate)
        opacityAnimator(zoomButtonsWidget)->animateTo(buttonsOpacity);
    else
        zoomButtonsWidget->setOpacity(buttonsOpacity);
}

bool TimelineWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

void TimelineWorkbenchPanel::setShowButtonUsed(bool used)
{
    showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void TimelineWorkbenchPanel::at_sliderResizerWidget_wheelEvent(QObject* /*target*/, QEvent* event)
{
    auto oldEvent = static_cast<QGraphicsSceneWheelEvent *>(event);
    QGraphicsSceneWheelEvent newEvent(QEvent::GraphicsSceneWheel);
    newEvent.setDelta(oldEvent->delta());
    newEvent.setPos(item->timeSlider()->mapFromItem(resizerWidget, oldEvent->pos()));
    newEvent.setScenePos(oldEvent->scenePos());
    display()->scene()->sendEvent(item->timeSlider(), &newEvent);
}

} //namespace NxUi