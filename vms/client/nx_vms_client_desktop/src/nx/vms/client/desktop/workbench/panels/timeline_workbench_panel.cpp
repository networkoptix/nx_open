// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>

#include <client/client_runtime_settings.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/ui/scene/widgets/timeline_calendar_widget.h>
#include <nx/vms/client/desktop/workbench/timeline/control_widget.h>
#include <nx/vms/client/desktop/workbench/timeline/navigation_widget.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_loading_manager.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_panel.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/event_processors.h>

#include "../workbench_animations.h"
#include "buttons.h"
#include "calendar_workbench_panel.h"

using nx::vms::client::desktop::workbench::timeline::ThumbnailLoadingManager;

namespace {

static const int kVideoWallTimelineAutoHideTimeoutMs = 10000;

static const int kShowTimelineTimeoutMs = 100;
static const int kCloseTimelineTimeoutMs = 250;

static const int kResizerHeight = 8;

static const int kShowWidgetHeight = 50;
static const int kShowWidgetHiddenHeight = 12;

} // namespace

namespace nx::vms::client::desktop {

using namespace ui::workbench;

TimelineWorkbenchPanel::TimelineWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    item(new QnNavigationItem(parentWidget)),
    m_lastThumbnailsHeight(ThumbnailLoadingManager::kDefaultSize.height()),
    m_ignoreClickEvent(false),
    m_visible(false),
    m_isPinnedManually(false),
    m_resizing(false),
    m_updateResizerGeometryLater(false),
    m_autoHideHeight(0),
    m_navigationWidget(new workbench::timeline::NavigationWidget(windowContext(), mainWindow()->view())),
    m_controlWidget(new workbench::timeline::ControlWidget(windowContext(), mainWindow()->view())),
    m_pinButton(newPinTimelineButton(parentWidget, windowContext(),
        menu::PinTimelineAction)),
    m_showButton(newShowHideButton(parentWidget, windowContext(),
        menu::ToggleTimelineAction)),
    m_resizerWidget(new QnResizerWidget(Qt::Vertical, parentWidget)),
    m_showWidget(new GraphicsWidget(parentWidget)),
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

    {
        QTransform transform;
        transform.rotate(-90);
        m_showButton->setTransform(transform);
        m_pinButton->setTransform(transform);
    }

    connect(item, &QnNavigationItem::navigationPlaceholderGeometryChanged, this,
        [this] (const QRectF& rect)
        {
            QPointF newPos = rect.topLeft();
            newPos.setY(newPos.y() + (rect.height() - m_navigationWidget->height()) / 2);

            m_navigationWidget->move((item->pos() + newPos).toPoint());
        });

    connect(m_navigationWidget, &workbench::timeline::NavigationWidget::geometryChanged, this,
        [this]
        {
            item->setNavigationRectSize(m_navigationWidget->rect());
        });

    connect(item, &QnNavigationItem::controlPlaceholderGeometryChanged, this,
        [this] (const QRectF& rect)
        {
            QPointF newPos = rect.topLeft();
            newPos.setY(newPos.y() + (rect.height() - m_controlWidget->height()) / 2);

            m_controlWidget->move((item->pos() + newPos).toPoint());
        });

    connect(m_controlWidget, &workbench::timeline::ControlWidget::geometryChanged, this,
        [this]
        {
            item->setControlRectSize(m_controlWidget->rect());
        });

    item->setNavigationRectSize(m_navigationWidget->rect());
    item->setControlRectSize(m_controlWidget->rect());

    m_showButton->setFocusProxy(item);
    m_showButton->setZValue(ControlItemZOrder);

    m_pinButton->setFocusProxy(item);
    m_pinButton->setZValue(ControlItemZOrder);
    m_pinButton->setVisible(false);

    action(menu::ToggleTimelineAction)->setData(settings.state == Qn::PaneState::Opened);

    connect(action(menu::ToggleTimelineAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!isPinned())
            {
                m_pinButton->setVisible(checked);
                m_showButton->setVisible(!checked);
            }

            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    connect(action(menu::PinTimelineAction), &QAction::triggered, this,
        [this]
        {
            m_isPinnedManually = true;

            m_showButton->setVisible(true);
            m_pinButton->setVisible(false);
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
    m_showingProcessor->addTargetItem(m_pinButton);
    m_showingProcessor->addTargetItem(m_showWidget);
    m_showingProcessor->addTargetItem(m_resizerWidget);
    m_showingProcessor->setHoverEnterDelay(kShowTimelineTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]
        {
            if (!isPinned() && !isOpened())
            {
                // TODO: #sivanov Process handlers.
                setOpened(true);

                /* So that the click that may follow won't hide it. */
                setShowButtonUsed(false);
                QTimer::singleShot(kButtonInactivityTimeoutMs, this,
                    [this]
                    {
                        setShowButtonUsed(true);
                    });
            }

            m_hidingProcessor->forceHoverEnter();
        });

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(m_showButton);
    m_hidingProcessor->addTargetItem(m_pinButton);
    m_hidingProcessor->addTargetItem(m_resizerWidget);
    m_hidingProcessor->addTargetItem(m_showWidget);
    m_hidingProcessor->addTargetItem(item->timeSlider()->bookmarksViewer());
    m_hidingProcessor->addTargetWidget(item->timeSlider()->tooltip()->widget());
    m_hidingProcessor->addTargetWidget(m_navigationWidget);
    m_hidingProcessor->addTargetWidget(m_controlWidget);
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
                setOpened(false); // TODO: #sivanov Process handlers.
        });
    connect(menu(), &nx::vms::client::desktop::menu::Manager::menuAboutToHide, m_hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);

    connect(action(menu::ToggleThumbnailsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            setThumbnailsVisible(checked);
        });

    if (qnRuntime->isVideoWallMode())
    {
        m_showButton->setVisible(false);
        m_pinButton->setVisible(false);
        m_autoHideTimer = new QTimer(this);
        connect(m_autoHideTimer, &QTimer::timeout, this,
            [this]
            {
                setVisible(false, true);
            });
    }

    enum { kSliderLeaveTimeout = 100 };
    m_opacityProcessor->setHoverLeaveDelay(kSliderLeaveTimeout);
    item->timeSlider()->bookmarksViewer()->setTimelineHoverProcessor(m_opacityProcessor);
    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(m_showButton);
    m_opacityProcessor->addTargetItem(m_pinButton);
    m_opacityProcessor->addTargetItem(m_resizerWidget);
    m_opacityProcessor->addTargetItem(m_showWidget);
    m_opacityProcessor->addTargetWidget(m_navigationWidget);
    m_opacityProcessor->addTargetWidget(m_controlWidget);
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
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_pinButton));

    auto sliderWheelEater = new QnSingleEventEater(this);
    sliderWheelEater->setEventType(QEvent::GraphicsSceneWheel);
    m_resizerWidget->installEventFilter(sliderWheelEater);

    // TODO: #vkutin Check if this still works (installSceneEventFilter might be required).
    installEventHandler(m_resizerWidget, QEvent::GraphicsSceneWheel, this,
        &TimelineWorkbenchPanel::at_sliderResizerWidget_wheelEvent);

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(parentWidget, item, Qt::TopEdge, kShadowThickness);
    shadow->setZValue(ShadowItemZOrder);

    connect(action(menu::ResourcesModeAction), &QAction::toggled,
        this, &TimelineWorkbenchPanel::updateTooltipVisibility);

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
        m_opacityProcessor->removeTargetWidget(m_calendar->widget());
        m_hidingProcessor->removeTargetWidget(m_calendar->widget());
    }

    m_calendar = calendar;

    auto updateAutoHideHeight =
        [this]()
        {
            int autoHideHeight = m_calendar && m_calendar->isVisible()
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

    connect(m_calendar.data(), &AbstractWorkbenchPanel::visibleChanged, this, updateAutoHideHeight);
    connect(m_calendar.data(), &AbstractWorkbenchPanel::geometryChanged, this, updateAutoHideHeight);

    m_opacityProcessor->addTargetWidget(m_calendar->widget());
    m_hidingProcessor->addTargetWidget(m_calendar->widget());
}

bool TimelineWorkbenchPanel::isPinned() const
{
    return (display()->widget(Qn::ZoomedRole) == nullptr) || m_isPinnedManually;
}

bool TimelineWorkbenchPanel::isPinnedManually() const
{
    return m_isPinnedManually;
}

bool TimelineWorkbenchPanel::isOpened() const
{
    return action(menu::ToggleTimelineAction)->isChecked();
}

void TimelineWorkbenchPanel::setOpened(bool opened, bool animate)
{
    if ((display()->widget(Qn::ZoomedRole) != nullptr) && !opened)
        m_isPinnedManually = false;

    ensureAnimationAllowed(&animate);

    if (!item)
        return;

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QScopedValueRollback<bool> guard(m_ignoreClickEvent, true);
    action(menu::ToggleTimelineAction)->setChecked(opened);

    m_yAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(m_yAnimator, opened
        ? Animations::Id::TimelineExpand
        : Animations::Id::TimelineCollapse);

    auto parentWidgetRect = m_parentWidget->rect();
    qreal newY = parentWidgetRect.bottom()
        + (opened ? -item->size().height() : kHidePanelOffset);

    if (!opened)
    {
        const auto handleClosed =
            [this]()
            {
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
    updateTooltipVisibility();
    emit openedChanged(opened, animate);
}

void TimelineWorkbenchPanel::updateTooltipVisibility()
{
    const bool tooltipsVisible = isOpened()
        && m_visible
        && action(menu::ResourcesModeAction)->isChecked()
        && !qnRuntime->isVideoWallMode();

    item->timeSlider()->setTooltipVisible(tooltipsVisible);
    item->timeSlider()->setLivePreviewAllowed(tooltipsVisible);
    m_navigationWidget->setTooltipsVisible(tooltipsVisible);
    m_controlWidget->setTooltipsVisible(tooltipsVisible);
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

    m_navigationWidget->setVisible(visible);
    m_controlWidget->setVisible(visible);
    updateTooltipVisibility();

    updateGeometry();
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
        auto pinButtonAnimator = opacityAnimator(m_pinButton);

        m_opacityAnimatorGroup->pause();

        itemAnimator->setTargetValue(opacity * masterOpacity());
        buttonAnimator->setTargetValue(opacity);
        pinButtonAnimator->setTargetValue(opacity);

        for (auto animator: { itemAnimator, buttonAnimator, pinButtonAnimator })
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
        m_pinButton->setOpacity(opacity);
    }

    m_resizerWidget->setVisible(visible);
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

QRectF TimelineWorkbenchPanel::geometry() const
{
    return item->geometry();
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
        m_lastThumbnailsHeight = item->geometry().height() - height;
    else
        height += m_lastThumbnailsHeight;

    QRectF geometry = item->geometry();
    geometry.setHeight(height);
    item->setGeometry(geometry);

    // Fix Y coordinate.
    setOpened(/*opened*/ true, /*animate*/ false);
    action(menu::ToggleThumbnailsAction)->setChecked(visible);
}

void TimelineWorkbenchPanel::setThumbnailsState(bool visible, qreal lastThumbnailsHeight)
{
    qreal height = minimumHeight();
    if (visible)
        height += lastThumbnailsHeight;

    m_lastThumbnailsHeight = lastThumbnailsHeight;

    QRectF geometry = item->geometry();
    geometry.setHeight(height);
    item->setGeometry(geometry);

    action(menu::ToggleThumbnailsAction)->setChecked(visible);
}

qreal TimelineWorkbenchPanel::thumbnailsHeight() const
{
    return isThumbnailsVisible()
        ? item->geometry().height() - minimumHeight()
        : m_lastThumbnailsHeight;
}

void TimelineWorkbenchPanel::setPanelSize(qreal /*size*/)
{
    // Nothing to do here. Timeline adjusts its size automatically.
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
    m_pinButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void TimelineWorkbenchPanel::updateResizerGeometry()
{
    if (m_updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &TimelineWorkbenchPanel::updateResizerGeometry);
        return;
    }

    const workbench::timeline::ThumbnailPanel* thumbnailPanel = item->thumbnailPanel();
    const QRectF thumbnailPanelRect = thumbnailPanel->rect();

    QRectF resizerGeometry = QRectF(
        m_parentWidget->mapFromItem(thumbnailPanel, thumbnailPanelRect.topLeft()),
        m_parentWidget->mapFromItem(thumbnailPanel, thumbnailPanelRect.topRight()));

    resizerGeometry.setHeight(kResizerHeight);

    if (!qFuzzyEquals(resizerGeometry, m_resizerWidget->geometry()))
    {
        QScopedValueRollback<bool> guard(m_updateResizerGeometryLater, true);

        m_resizerWidget->setGeometry(resizerGeometry);

        // This one is needed here as we are in a handler and thus geometry change does not adjust
        // position.
        m_resizerWidget->setPos(resizerGeometry.topLeft());
    }
}

void TimelineWorkbenchPanel::updateControlsGeometry()
{
    if (m_resizing)
        return;

    QScopedValueRollback<bool> guard(m_resizing, true);

    QRectF geometry = item->geometry();
    auto parentWidgetRect = m_parentWidget->rect();

    /* Button is painted rotated, so we taking into account its height, not width. */
    QPointF showButtonPos(
        (geometry.left() + geometry.right() - m_showButton->geometry().height()) / 2,
        qMin(parentWidgetRect.bottom(), geometry.top()));
    m_showButton->setPos(showButtonPos);
    m_pinButton->setPos(showButtonPos);

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

    if (isThumbnailsVisible())
    {
        m_lastThumbnailsHeight = item->geometry().height() - minimumHeight();
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

    if (!m_resizerWidget->isBeingMoved())
        return;

    qreal y = mainWindow()->view()->mapFromGlobal(QCursor::pos()).y();
    qreal parentBottom = m_parentWidget->rect().bottom();
    qreal targetHeight = parentBottom - y;
    qreal minHeight = minimumHeight();
    qreal jmpHeight = minHeight + ThumbnailLoadingManager::kMinimumSize.height();
    qreal maxHeight = minHeight + ThumbnailLoadingManager::kMaximumHeight;

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
    action(menu::ToggleThumbnailsAction)->setChecked(isThumbnailsVisible());
}

void TimelineWorkbenchPanel::at_sliderResizerWidget_wheelEvent(QObject* /*target*/, QEvent* event)
{
    auto oldEvent = static_cast<QGraphicsSceneWheelEvent *>(event);
    QGraphicsSceneWheelEvent newEvent(QEvent::GraphicsSceneWheel);
    newEvent.setDelta(oldEvent->delta());
    newEvent.setPos(item->timeSlider()->mapFromItem(m_resizerWidget, oldEvent->pos()));
    newEvent.setScenePos(oldEvent->scenePos());
    mainWindow()->scene()->sendEvent(item->timeSlider(), &newEvent);
}

} //namespace nx::vms::client::desktop
