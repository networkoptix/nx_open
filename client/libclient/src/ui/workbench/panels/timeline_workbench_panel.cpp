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
#include <utils/common/scoped_value_rollback.h>

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
    zoomingIn(false),
    zoomingOut(false),
    yAnimator(new VariantAnimator(this)),
    showButton(NxUi::newShowHideButton(parentWidget, context(),
        action(QnActions::ToggleSliderAction))),
    showWidget(new GraphicsWidget(parentWidget)),
    m_autoHideTimer(nullptr),
    lastThumbnailsHeight(kDefaultThumbnailsHeight),

    m_visible(false),
    m_opened(false),
    m_ignoreResizerGeometryChanges(false),
    m_updateResizerGeometryLater(false),
    m_resizerWidget(new QnResizerWidget(Qt::Vertical, parentWidget)),
    m_zoomButtonsWidget(new GraphicsWidget(parentWidget)),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this))
{
    m_resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    m_resizerWidget->setZValue(ResizerItemZOrder);
    connect(m_resizerWidget, &QGraphicsWidget::geometryChanged, this,
        &TimelineWorkbenchPanel::at_resizerWidget_geometryChanged);

    item->setProperty(Qn::NoHandScrollOver, true);
    item->setZValue(ContentItemZOrder);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &TimelineWorkbenchPanel::updateControlsGeometry);

    action(QnActions::ToggleSliderAction)->setChecked(settings.state == Qn::PaneState::Opened);
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
    m_showingProcessor->addTargetItem(m_resizerWidget);
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
    m_hidingProcessor->addTargetItem(m_resizerWidget);
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
        m_autoHideTimer = new QTimer(this);
        connect(m_autoHideTimer, &QTimer::timeout, this,
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

    m_zoomButtonsWidget->setLayout(sliderZoomButtonsLayout);
    m_zoomButtonsWidget->setOpacity(0.0);

    m_zoomButtonsWidget->setVisible(navigator()->hasArchive());
    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged, this,
        [this]()
        {
            m_zoomButtonsWidget->setVisible(navigator()->hasArchive());
        });

    enum { kSliderLeaveTimeout = 100 };
    m_opacityProcessor->setHoverLeaveDelay(kSliderLeaveTimeout);
    item->timeSlider()->bookmarksViewer()->setHoverProcessor(m_opacityProcessor);
    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(item->timeSlider()->toolTipItem());
    m_opacityProcessor->addTargetItem(showButton);
    m_opacityProcessor->addTargetItem(m_resizerWidget);
    m_opacityProcessor->addTargetItem(showWidget);
    m_opacityProcessor->addTargetItem(m_zoomButtonsWidget);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    yAnimator->setTimer(animationTimer());
    yAnimator->setTargetObject(item);
    yAnimator->setAccessor(new PropertyAccessor("y"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(showButton));

    auto sliderWheelEater = new QnSingleEventEater(this);
    sliderWheelEater->setEventType(QEvent::GraphicsSceneWheel);
    m_resizerWidget->installEventFilter(sliderWheelEater);

    auto sliderWheelSignalizer = new QnSingleEventSignalizer(this);
    sliderWheelSignalizer->setEventType(QEvent::GraphicsSceneWheel);
    m_resizerWidget->installEventFilter(sliderWheelSignalizer);

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
    m_resizerWidget->setEnabled(opened);

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

    if (m_autoHideTimer)
    {
        if (visible)
            m_autoHideTimer->start(kVideoWallTimelineAutoHideTimeoutMs);
        else
            m_autoHideTimer->stop();
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

    m_resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

void TimelineWorkbenchPanel::updateOpacity(bool animate)
{
    base_type::updateOpacity(animate);

    bool isButtonOpaque = m_visible && m_opacityProcessor->isHovered();
    const qreal buttonsOpacity = isButtonOpaque ? NxUi::kOpaque : NxUi::kHidden;

    ensureAnimationAllowed(&animate);
    if (animate)
        opacityAnimator(m_zoomButtonsWidget)->animateTo(buttonsOpacity);
    else
        m_zoomButtonsWidget->setOpacity(buttonsOpacity);
}

bool TimelineWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

bool TimelineWorkbenchPanel::isThumbnailsVisible() const
{
    qreal height = item->geometry().height();
    return !qFuzzyIsNull(height)
        && !qFuzzyCompare(height, item->effectiveSizeHint(Qt::MinimumSize).height());
}

void TimelineWorkbenchPanel::setThumbnailsVisible(bool visible)
{
    if (visible == isThumbnailsVisible())
        return;

    qreal sliderHeight = item->effectiveSizeHint(Qt::MinimumSize).height();
    if (!visible)
        lastThumbnailsHeight = item->geometry().height() - sliderHeight;
    else
        sliderHeight += lastThumbnailsHeight;

    QRectF geometry = item->geometry();
    geometry.setHeight(sliderHeight);
    item->setGeometry(geometry);
}

void TimelineWorkbenchPanel::setShowButtonUsed(bool used)
{
    showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void TimelineWorkbenchPanel::updateResizerGeometry()
{
    if (m_updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &TimelineWorkbenchPanel::updateResizerGeometry);
        return;
    }

    QnTimeSlider *timeSlider = item->timeSlider();
    QRectF timeSliderRect = timeSlider->rect();

    QRectF sliderResizerGeometry = QRectF(
        m_parentWidget->mapFromItem(timeSlider, timeSliderRect.topLeft()),
        m_parentWidget->mapFromItem(timeSlider, timeSliderRect.topRight()));

    sliderResizerGeometry.moveTo(sliderResizerGeometry.topLeft() - QPointF(0, 8));
    sliderResizerGeometry.setHeight(16);

    if (!qFuzzyEquals(sliderResizerGeometry, m_resizerWidget->geometry()))
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updateResizerGeometryLater, true);

        m_resizerWidget->setGeometry(sliderResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_resizerWidget->setPos(sliderResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void TimelineWorkbenchPanel::updateControlsGeometry()
{
    QRectF geometry = item->geometry();
    auto parentWidgetRect = m_parentWidget->rect();

    /* Button is painted rotated, so we taking into account its height, not width. */
    QPointF showButtonPos(
        (geometry.left() + geometry.right() - showButton->geometry().height()) / 2,
        qMin(parentWidgetRect.bottom(), geometry.top()));
    showButton->setPos(showButtonPos);

    static const int kShowWidgetHeight = 50;
    static const int kShowWidgetHiddenHeight = 12;

    const int showWidgetY = isOpened()
        ? geometry.top() - kShowWidgetHeight
        : parentWidgetRect.bottom() - kShowWidgetHiddenHeight;

    QRectF showWidgetGeometry(geometry);
    showWidgetGeometry.setTop(showWidgetY);
    showWidgetGeometry.setHeight(kShowWidgetHeight);
    showWidget->setGeometry(showWidgetGeometry);

    auto zoomButtonsPos = item->timeSlider()->mapToItem(m_parentWidget,
        item->timeSlider()->rect().topLeft());
    m_zoomButtonsWidget->setPos(zoomButtonsPos);

    if (isThumbnailsVisible())
    {
        qreal sliderHeight = item->effectiveSizeHint(Qt::MinimumSize).height();
        lastThumbnailsHeight = item->geometry().height() - sliderHeight;
    }

    updateResizerGeometry();

    emit geometryChanged();
}

void TimelineWorkbenchPanel::at_resizerWidget_geometryChanged()
{
    if (m_ignoreResizerGeometryChanges)
        return;

    QRectF sliderResizerGeometry = m_resizerWidget->geometry();
    if (!sliderResizerGeometry.isValid())
    {
        updateResizerGeometry();
        return;
    }

    QRectF sliderGeometry = item->geometry();

    qreal targetHeight = sliderGeometry.bottom() - sliderResizerGeometry.center().y();
    qreal minHeight = item->effectiveSizeHint(Qt::MinimumSize).height();
    qreal jmpHeight = minHeight + 48.0;
    qreal maxHeight = minHeight + 196.0;

    if (targetHeight < (minHeight + jmpHeight) / 2)
        targetHeight = minHeight;
    else if (targetHeight < jmpHeight)
        targetHeight = jmpHeight;
    else if (targetHeight > maxHeight)
        targetHeight = maxHeight;

    if (!qFuzzyCompare(sliderGeometry.height(), targetHeight))
    {
        qreal sliderTop = sliderGeometry.top();
        sliderGeometry.setHeight(targetHeight);
        sliderGeometry.moveTop(sliderTop);

        QN_SCOPED_VALUE_ROLLBACK(&m_ignoreResizerGeometryChanges, true);
        item->setGeometry(sliderGeometry);
    }

    updateResizerGeometry();

    action(QnActions::ToggleThumbnailsAction)->setChecked(isThumbnailsVisible());
}

void TimelineWorkbenchPanel::at_sliderResizerWidget_wheelEvent(QObject* /*target*/, QEvent* event)
{
    auto oldEvent = static_cast<QGraphicsSceneWheelEvent *>(event);
    QGraphicsSceneWheelEvent newEvent(QEvent::GraphicsSceneWheel);
    newEvent.setDelta(oldEvent->delta());
    newEvent.setPos(item->timeSlider()->mapFromItem(m_resizerWidget, oldEvent->pos()));
    newEvent.setScenePos(oldEvent->scenePos());
    display()->scene()->sendEvent(item->timeSlider(), &newEvent);
}

} //namespace NxUi