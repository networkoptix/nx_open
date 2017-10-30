#include "timeline_workbench_panel.h"

#include <QtCore/QTimer>

#include <QtWidgets/QAction>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QMenu>

#include <client/client_runtime_settings.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>
#include <ui/workbench/panels/calendar_workbench_panel.h>
#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop::ui;

namespace {

static const int kDefaultThumbnailsHeight = 48;
static const int kVideoWallTimelineAutoHideTimeoutMs = 10000;

static const int kShowTimelineTimeoutMs = 100;
static const int kCloseTimelineTimeoutMs = 250;

static const int kMinThumbnailsHeight = 48;
static const int kMaxThumbnailsHeight = 196;

static const int kResizerHeight = 8;

static const int kShowWidgetHeight = 50;
static const int kShowWidgetHiddenHeight = 12;

}

namespace NxUi {

using namespace nx::client::desktop::ui::workbench;

TimelineWorkbenchPanel::TimelineWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    item(new QnNavigationItem(parentWidget)),
    zoomingIn(false),
    zoomingOut(false),
    lastThumbnailsHeight(kDefaultThumbnailsHeight),
    m_ignoreClickEvent(false),
    m_visible(false),
    m_resizing(false),
    m_updateResizerGeometryLater(false),
    m_autoHideHeight(0),
    m_showButton(NxUi::newShowHideButton(parentWidget, context(),
        action::ToggleTimelineAction)),
    m_resizerWidget(new QnResizerWidget(Qt::Vertical, parentWidget)),
    m_showWidget(new GraphicsWidget(parentWidget)),
    m_zoomButtonsWidget(new GraphicsWidget(parentWidget)),
    m_autoHideTimer(nullptr),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this)),
    m_yAnimator(new VariantAnimator(this))
{
    m_resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    m_resizerWidget->setProperty(Qn::BlockMotionSelection, true);
    m_resizerWidget->setZValue(ResizerItemZOrder);
    connect(m_resizerWidget, &QGraphicsWidget::geometryChanged, this,
        &TimelineWorkbenchPanel::at_resizerWidget_geometryChanged);

    item->setProperty(Qn::NoHandScrollOver, true);
    item->setProperty(Qn::BlockMotionSelection, true);
    item->setZValue(ContentItemZOrder);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &TimelineWorkbenchPanel::updateControlsGeometry);
    connect(item->timeSlider(), &QGraphicsWidget::geometryChanged, this,
        &TimelineWorkbenchPanel::updateControlsGeometry);

    action(action::ToggleTimelineAction)->setChecked(settings.state == Qn::PaneState::Opened);
    {
        QTransform transform;
        transform.rotate(-90);
        m_showButton->setTransform(transform);
    }
    m_showButton->setFocusProxy(item);
    m_showButton->setZValue(ControlItemZOrder);
    connect(action(action::ToggleTimelineAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    m_showWidget->setProperty(Qn::NoHandScrollOver, true);
    m_showWidget->setProperty(Qn::BlockMotionSelection, true);
    m_showWidget->setFlag(QGraphicsItem::ItemHasNoContents, true);
    m_showWidget->setVisible(false);
    m_showWidget->setZValue(BackgroundItemZOrder);

    auto handleWidgetChanged =
        [this](Qn::ItemRole role)
        {
            if (role == Qn::ZoomedRole)
                m_showWidget->setVisible(!isPinned());
        };

    connect(display(), &QnWorkbenchDisplay::widgetChanged,
        this, handleWidgetChanged, Qt::QueuedConnection /*queue past setOpened(false)*/);

    m_showingProcessor->addTargetItem(m_showButton);
    m_showingProcessor->addTargetItem(m_showWidget);
    m_showingProcessor->addTargetItem(m_resizerWidget);
    m_showingProcessor->setHoverEnterDelay(kShowTimelineTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]
        {
            if (!isPinned() && !isOpened())
            {
                // TODO: #GDM #high process handlers
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
    m_hidingProcessor->addTargetItem(m_showButton);
    m_hidingProcessor->addTargetItem(m_resizerWidget);
    m_hidingProcessor->addTargetItem(m_showWidget);
    m_hidingProcessor->addTargetItem(m_zoomButtonsWidget);
    m_hidingProcessor->addTargetItem(item->timeSlider()->toolTipItem());
    m_hidingProcessor->addTargetItem(item->timeSlider()->bookmarksViewer());
    m_hidingProcessor->addTargetItem(item->speedSlider()->toolTipItem());
    m_hidingProcessor->addTargetItem(item->volumeSlider()->toolTipItem());
    m_hidingProcessor->setHoverLeaveDelay(kCloseTimelineTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(kCloseTimelineTimeoutMs);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            /* Do not auto-hide slider if we have opened context menu. */
            const auto zoomedWidget = display()->widget(Qn::ZoomedRole);
            const bool zoomedMotionSearch =
                zoomedWidget && zoomedWidget->options().testFlag(QnResourceWidget::DisplayMotion);
            if (!zoomedMotionSearch && !isPinned() && isOpened() && !menu()->isMenuVisible())
                setOpened(false); // TODO: #GDM #high process handlers
        });
    connect(menu(), &nx::client::desktop::ui::action::Manager::menuAboutToHide, m_hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);

    connect(action(action::ToggleThumbnailsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            setThumbnailsVisible(checked);
        });

    if (qnRuntime->isVideoWallMode())
    {
        m_showButton->setVisible(false);
        m_autoHideTimer = new QTimer(this);
        connect(m_autoHideTimer, &QTimer::timeout, this,
            [this]
            {
                setVisible(false, true);
            });
    }

    item->timeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    item->timeSlider()->toolTipItem()->setProperty(Qn::BlockMotionSelection, true);
    item->timeSlider()->toolTipItem()->setZValue(NxUi::TooltipItemZOrder);
    item->speedSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    item->speedSlider()->toolTipItem()->setProperty(Qn::BlockMotionSelection, true);
    item->speedSlider()->toolTipItem()->setZValue(NxUi::TooltipItemZOrder);
    item->volumeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    item->volumeSlider()->toolTipItem()->setProperty(Qn::BlockMotionSelection, true);
    item->volumeSlider()->toolTipItem()->setZValue(NxUi::TooltipItemZOrder);

    auto sliderZoomOutButton = new QnImageButtonWidget();
    sliderZoomOutButton->setIcon(qnSkin->icon("slider/buttons/zoom_out.png"));
    sliderZoomOutButton->setPreferredSize(19, 16);
    context()->statisticsModule()->registerButton(lit("slider_zoom_out"), sliderZoomOutButton);

    auto sliderZoomInButton = new QnImageButtonWidget();
    sliderZoomInButton->setIcon(qnSkin->icon("slider/buttons/zoom_in.png"));
    sliderZoomInButton->setPreferredSize(19, 16);
    context()->statisticsModule()->registerButton(lit("slider_zoom_in"), sliderZoomInButton);

    auto sliderZoomButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    sliderZoomButtonsLayout->setSpacing(0.0);
    sliderZoomButtonsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    sliderZoomButtonsLayout->addItem(sliderZoomOutButton);
    sliderZoomButtonsLayout->addItem(sliderZoomInButton);

    m_zoomButtonsWidget->setLayout(sliderZoomButtonsLayout);
    m_zoomButtonsWidget->setOpacity(0.0);
    m_zoomButtonsWidget->setZValue(NxUi::ControlItemZOrder);

    m_zoomButtonsWidget->setVisible(navigator()->isTimelineRelevant());
    connect(navigator(), &QnWorkbenchNavigator::timelineRelevancyChanged, this,
        [this](bool value)
        {
            m_zoomButtonsWidget->setVisible(value);
        });

    enum { kSliderLeaveTimeout = 100 };
    m_opacityProcessor->setHoverLeaveDelay(kSliderLeaveTimeout);
    item->timeSlider()->bookmarksViewer()->setHoverProcessor(m_opacityProcessor);
    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(item->timeSlider()->toolTipItem());
    m_opacityProcessor->addTargetItem(m_showButton);
    m_opacityProcessor->addTargetItem(m_resizerWidget);
    m_opacityProcessor->addTargetItem(m_showWidget);
    m_opacityProcessor->addTargetItem(m_zoomButtonsWidget);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_yAnimator->setTimer(animationTimer());
    m_yAnimator->setTargetObject(item);
    m_yAnimator->setAccessor(new PropertyAccessor("y"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));

    auto sliderWheelEater = new QnSingleEventEater(this);
    sliderWheelEater->setEventType(QEvent::GraphicsSceneWheel);
    m_resizerWidget->installEventFilter(sliderWheelEater);

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

    // TODO: #vkutin #GDM #common Check if this still works (installSceneEventFilter might be required)
    installEventHandler(m_resizerWidget, QEvent::GraphicsSceneWheel, this,
        &TimelineWorkbenchPanel::at_sliderResizerWidget_wheelEvent);

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(parentWidget, item, Qt::TopEdge, NxUi::kShadowThickness);
    shadow->setZValue(NxUi::ShadowItemZOrder);

    updateGeometry();
}

TimelineWorkbenchPanel::~TimelineWorkbenchPanel()
{
}

void TimelineWorkbenchPanel::setCalendarPanel(CalendarWorkbenchPanel* calendar)
{
    if (m_calendar == calendar)
        return;

    if (m_calendar)
    {
        m_calendar->disconnect(this);
        m_calendar->hidingProcessor->removeTargetItem(item->calendarButton());

        for (auto item : m_calendar->activeItems())
        {
            m_opacityProcessor->removeTargetItem(item);
            m_hidingProcessor->removeTargetItem(item);
        }
    }

    m_calendar = calendar;

    auto updateAutoHideHeight =
        [this]()
        {
            auto autoHideHeight = m_calendar && m_calendar->isVisible()
                ? effectiveGeometry().bottom() - m_calendar->effectiveGeometry().top() + 1
                : 0;

            if (m_autoHideHeight == autoHideHeight)
                return;

            m_autoHideHeight = autoHideHeight;
            updateControlsGeometry();
        };

    updateAutoHideHeight();

    if (!m_calendar)
        return;

    connect(m_calendar.data(), &NxUi::AbstractWorkbenchPanel::visibleChanged, this, updateAutoHideHeight);
    connect(m_calendar.data(), &NxUi::AbstractWorkbenchPanel::geometryChanged, this, updateAutoHideHeight);

    m_calendar->hidingProcessor->addTargetItem(item->calendarButton());

    for (auto item : m_calendar->activeItems())
    {
        m_opacityProcessor->addTargetItem(item);
        m_hidingProcessor->addTargetItem(item);
    }
}

bool TimelineWorkbenchPanel::isPinned() const
{
    return display()->widget(Qn::ZoomedRole) == nullptr;
}

bool TimelineWorkbenchPanel::isOpened() const
{
    return action(action::ToggleTimelineAction)->isChecked();
}

void TimelineWorkbenchPanel::setOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (!item)
        return;

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(action::ToggleTimelineAction)->setChecked(opened);

    m_yAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(m_yAnimator, opened
        ? Animations::Id::TimelineExpand
        : Animations::Id::TimelineCollapse);

    auto parentWidgetRect = m_parentWidget->rect();
    qreal newY = parentWidgetRect.bottom()
        + (opened ? -item->size().height() : kHidePanelOffset);

    if (opened)
    {
        item->speedSlider()->setToolTipEnabled(true);
        item->volumeSlider()->setToolTipEnabled(true);
    }
    else
    {
        const auto handleClosed =
            [this]()
            {
                item->speedSlider()->setToolTipEnabled(false);
                item->volumeSlider()->setToolTipEnabled(false);
                disconnect(m_yAnimator, &VariantAnimator::finished, this, nullptr);
            };

        if (animate)
            connect(m_yAnimator, &VariantAnimator::finished, this, handleClosed);
        else
            handleClosed();
    }

    if (animate)
        m_yAnimator->animateTo(newY);
    else
        item->setY(newY);

    m_resizerWidget->setEnabled(opened);
    item->timeSlider()->setTooltipVisible(opened);

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
    return opacityAnimator(m_showButton)->targetValue().toDouble();
}

void TimelineWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);
    bool visible = !qFuzzyIsNull(opacity);

    if (animate)
    {
        auto itemAnimator = opacityAnimator(item);
        auto buttonAnimator = opacityAnimator(m_showButton);

        m_opacityAnimatorGroup->pause();

        itemAnimator->setTargetValue(opacity * masterOpacity());
        buttonAnimator->setTargetValue(opacity);

        for (auto animator: { itemAnimator, buttonAnimator })
        {
            qnWorkbenchAnimations->setupAnimator(animator, visible
                ? Animations::Id::TimelineShow
                : Animations::Id::TimelineHide);
        }

        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity * masterOpacity());
        m_showButton->setOpacity(opacity);
    }

    m_resizerWidget->setVisible(visible);
}

void TimelineWorkbenchPanel::updateOpacity(bool animate)
{
    base_type::updateOpacity(animate);

    bool buttonsVisible = m_visible && m_opacityProcessor->isHovered();
    const qreal buttonsOpacity = buttonsVisible ? NxUi::kOpaque : NxUi::kHidden;

    ensureAnimationAllowed(&animate);
    if (animate)
    {
        auto animator = opacityAnimator(m_zoomButtonsWidget);
        qnWorkbenchAnimations->setupAnimator(animator, buttonsVisible
            ? Animations::Id::TimelineButtonsShow
            : Animations::Id::TimelineButtonsHide);
        animator->animateTo(buttonsOpacity * masterOpacity());
    }
    else
    {
        if (hasOpacityAnimator(m_zoomButtonsWidget))
            opacityAnimator(m_zoomButtonsWidget)->stop();
        m_zoomButtonsWidget->setOpacity(buttonsOpacity * masterOpacity());
    }
}

bool TimelineWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

QRectF TimelineWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (m_yAnimator->isRunning())
        geometry.moveTop(m_yAnimator->targetValue().toReal());
    return geometry;
}

void TimelineWorkbenchPanel::stopAnimations()
{
    if (!m_yAnimator->isRunning())
        return;

    m_yAnimator->stop();
    item->setY(m_yAnimator->targetValue().toDouble());
}

bool TimelineWorkbenchPanel::isThumbnailsVisible() const
{
    qreal height = item->geometry().height();
    return !qFuzzyIsNull(height)
        && !qFuzzyCompare(height, minimumHeight());
}

void TimelineWorkbenchPanel::setThumbnailsVisible(bool visible)
{
    if (visible == isThumbnailsVisible())
        return;

    qreal height = minimumHeight();
    if (!visible)
        lastThumbnailsHeight = item->geometry().height() - height;
    else
        height += lastThumbnailsHeight;

    QRectF geometry = item->geometry();
    geometry.setHeight(height);
    item->setGeometry(geometry);

    /* Fix y coord. */
    setOpened(true, false);
}

void TimelineWorkbenchPanel::updateGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();
    qreal newY = parentWidgetRect.bottom()
        + (isOpened() ? -item->size().height() : kHidePanelOffset);

    item->setGeometry(QRectF(
        0.0,
        newY,
        parentWidgetRect.width(),
        item->size().height()));
}

void TimelineWorkbenchPanel::setShowButtonUsed(bool used)
{
    m_showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
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

    QRectF resizerGeometry = QRectF(
        m_parentWidget->mapFromItem(timeSlider, timeSliderRect.topLeft()),
        m_parentWidget->mapFromItem(timeSlider, timeSliderRect.topRight()));

    resizerGeometry.setHeight(kResizerHeight);

    if (!qFuzzyEquals(resizerGeometry, m_resizerWidget->geometry()))
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updateResizerGeometryLater, true);

        m_resizerWidget->setGeometry(resizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_resizerWidget->setPos(resizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void TimelineWorkbenchPanel::updateControlsGeometry()
{
    if (m_resizing)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_resizing, true);

    QRectF geometry = item->geometry();
    auto parentWidgetRect = m_parentWidget->rect();

    /* Button is painted rotated, so we taking into account its height, not width. */
    QPointF showButtonPos(
        (geometry.left() + geometry.right() - m_showButton->geometry().height()) / 2,
        qMin(parentWidgetRect.bottom(), geometry.top()));
    m_showButton->setPos(showButtonPos);

    int showWidgetHeight = qMax(
        m_autoHideHeight - geometry.height(),
        kShowWidgetHeight);

    const int showWidgetY = isOpened()
        ? geometry.top() - showWidgetHeight
        : parentWidgetRect.bottom() - kShowWidgetHiddenHeight;

    QRectF showWidgetGeometry(geometry);
    showWidgetGeometry.setTop(showWidgetY);
    showWidgetGeometry.setHeight(showWidgetHeight);
    m_showWidget->setGeometry(showWidgetGeometry);

    auto zoomButtonsPos = item->timeSlider()->mapToItem(m_parentWidget,
        item->timeSlider()->rect().topLeft());
    m_zoomButtonsWidget->setPos(zoomButtonsPos);

    if (isThumbnailsVisible())
    {
        lastThumbnailsHeight = item->geometry().height() - minimumHeight();
    }

    updateResizerGeometry();

    emit geometryChanged();
}

qreal TimelineWorkbenchPanel::minimumHeight() const
{
    return item->effectiveSizeHint(Qt::MinimumSize).height();
}

void TimelineWorkbenchPanel::at_resizerWidget_geometryChanged()
{
    if (m_resizing)
        return;

    QRectF resizerGeometry = m_resizerWidget->geometry();
    if (!resizerGeometry.isValid())
    {
        updateResizerGeometry();
        return;
    }

    qreal y = display()->view()->mapFromGlobal(QCursor::pos()).y();
    qreal parentBottom = m_parentWidget->rect().bottom();
    qreal targetHeight = parentBottom - y;
    qreal minHeight = minimumHeight();
    qreal jmpHeight = minHeight + kMinThumbnailsHeight;
    qreal maxHeight = minHeight + kMaxThumbnailsHeight;

    if (targetHeight < (minHeight + jmpHeight) / 2)
        targetHeight = minHeight;
    else if (targetHeight < jmpHeight)
        targetHeight = jmpHeight;
    else if (targetHeight > maxHeight)
        targetHeight = maxHeight;

    QRectF geometry = item->geometry();
    if (!qFuzzyEquals(geometry.height(), targetHeight))
    {
        qreal targetTop = parentBottom - targetHeight;
        geometry.setHeight(targetHeight);
        geometry.moveTop(targetTop);

        item->setGeometry(geometry);
    }

    updateResizerGeometry();
    action(action::ToggleThumbnailsAction)->setChecked(isThumbnailsVisible());
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
