#include "workbench_ui.h"

#include <cmath> /* For std::floor. */

#include <QtCore/QMargins>

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QAction>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <client/client_meta_types.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameter_types.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>

#include <ui/common/palette.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/gui_elements_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/main_window.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_pane_settings.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/workbench/panels/special_layout_panel.h>
#include <nx/vms/client/desktop/debug_utils/instruments/debug_info_instrument.h>

#include <utils/common/event_processors.h>
#include <nx/vms/client/desktop/ini.h>

#include "panels/resources_workbench_panel.h"
#include "panels/notifications_workbench_panel.h"
#include "panels/timeline_workbench_panel.h"
#include "panels/calendar_workbench_panel.h"
#include "panels/title_workbench_panel.h"

namespace nx::vms::client::desktop {

using namespace ui;

namespace {

Qn::PaneState makePaneState(bool opened, bool pinned = true)
{
    return pinned ? (opened ? Qn::PaneState::Opened : Qn::PaneState::Closed) : Qn::PaneState::Unpinned;
}

constexpr int kHideControlsTimeoutMs = 2000;

constexpr qreal kZoomedTimelineOpacity = 0.9;

} // namespace

static QtMessageHandler previousMsgHandler = 0;
static QnDebugProxyLabel* debugLabel = nullptr;

static void uiMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (previousMsgHandler)
        previousMsgHandler(type, ctx, msg);

    if (!debugLabel)
        return;

    debugLabel->appendTextQueued(msg);
}

WorkbenchUi::WorkbenchUi(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_instrumentManager(display()->instrumentManager())
{
    QGraphicsLayout::setInstantInvalidatePropagation(true);

    /* Install and configure instruments. */

    m_debugInfoInstrument = new DebugInfoInstrument(this);

    m_controlsActivityInstrument = new ActivityListenerInstrument(true, kHideControlsTimeoutMs, this);

    m_instrumentManager->installInstrument(m_debugInfoInstrument, InstallationMode::InstallBefore,
        display()->paintForwardingInstrument());
    m_instrumentManager->installInstrument(m_controlsActivityInstrument);

    connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityStopped, this,
        &WorkbenchUi::at_activityStopped);
    connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityResumed, this,
        &WorkbenchUi::at_activityStarted);
    connect(m_debugInfoInstrument, &DebugInfoInstrument::debugInfoChanged, this,
        [this](const QString& text)
        {
            m_fpsItem->setText(text);
            m_fpsItem->resize(m_fpsItem->effectiveSizeHint(Qt::PreferredSize));
        });

    /* Create controls. */
    createControlsWidget();

    /* Animation. */
    setTimer(m_instrumentManager->animationTimer());
    startListening();

    /* Fps counter. */
    createFpsWidget();

    QnPaneSettingsMap settings = qnSettings->paneStateSettings();

    if (qnRuntime->isDesktopMode())
    {
        createTreeWidget(settings[Qn::WorkbenchPane::Tree]); //< Tree panel.
        createTitleWidget(settings[Qn::WorkbenchPane::Title]); //< Title bar.
        createLayoutPanelWidget(settings[Qn::WorkbenchPane::SpecialLayout]); //< Special layout
        if (!qnRuntime->lightMode().testFlag(Qn::LightModeNoNotifications))
            createNotificationsWidget(settings[Qn::WorkbenchPane::Notifications]); //< Notifications
    }

    /* Calendar. */
    createCalendarWidget(settings[Qn::WorkbenchPane::Calendar]);

    /* Navigation slider. */
    createTimelineWidget(settings[Qn::WorkbenchPane::Navigation]);

    /* Make timeline panel aware of calendar panel. */
    m_timeline->setCalendarPanel(m_calendar);

    /* Windowed title shadow. */
    auto windowedTitleShadow = new QnEdgeShadowWidget(m_controlsWidget,
        m_controlsWidget, Qt::TopEdge, kShadowThickness, QnEdgeShadowWidget::kInner);
    windowedTitleShadow->setZValue(ShadowItemZOrder);

    /* Debug overlay */
    if (ini().developerMode && ini().developerGuiPanel)
        createDebugWidget();

    initGraphicsMessageBoxHolder();

    /* Connect to display. */
    display()->view()->addAction(action(action::FreespaceAction));
    connect(display(), &QnWorkbenchDisplay::widgetChanged, this,
        &WorkbenchUi::at_display_widgetChanged);

    /* After widget was removed we can possibly increase margins. */
    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
        &WorkbenchUi::updateViewportMarginsAnimated, Qt::QueuedConnection);

    connect(action(action::FreespaceAction), &QAction::triggered, this,
        &WorkbenchUi::at_freespaceAction_triggered);
    connect(action(action::EffectiveMaximizeAction), &QAction::triggered, this,
        [this, windowedTitleShadow]()
        {
            if (m_inFreespace)
                at_freespaceAction_triggered();

            windowedTitleShadow->setVisible(
                !action(action::EffectiveMaximizeAction)->isChecked());
        });

    /* Init fields. */
    setFlags(HideWhenNormal | HideWhenZoomed | AdjustMargins);

    setTimelineVisible(false, false);
    setTreeVisible(true, false);
    setTitleVisible(false, false);
    setNotificationsVisible(true, false);
    updateControlsVisibility(false);

    setTreeOpened(settings[Qn::WorkbenchPane::Tree].state == Qn::PaneState::Opened, false);
    setTitleOpened(settings[Qn::WorkbenchPane::Title].state == Qn::PaneState::Opened, false);
    setTimelineOpened(settings[Qn::WorkbenchPane::Navigation].state != Qn::PaneState::Closed, false);
    setNotificationsOpened(settings[Qn::WorkbenchPane::Notifications].state == Qn::PaneState::Opened, false);
    setCalendarOpened(settings[Qn::WorkbenchPane::Calendar].state == Qn::PaneState::Opened, false);

    m_timeline->lastThumbnailsHeight = settings[Qn::WorkbenchPane::Thumbnails].span;
    m_timeline->setThumbnailsVisible(settings[Qn::WorkbenchPane::Thumbnails].state == Qn::PaneState::Opened);

    connect(action(action::BeforeExitAction), &QAction::triggered, this,
        [this]
        {
            if (!m_inFreespace)
                storeSettings();
        });

    // TODO: #ynikitenkov before final commit: add currentLayoutFlags to the workbench()?
    // Possibly it should be usedd in several places
    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
        [this]()
        {
            const auto layout = workbench()->currentLayout();
            if (!layout)
                return;

            disconnect(layout, &QnWorkbenchLayout::flagsChanged,
                this, &WorkbenchUi::updateControlsVisibilityAnimated);
        });

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        [this]()
        {
            const auto layout = workbench()->currentLayout();
            if (!layout)
                return;

            connect(layout, &QnWorkbenchLayout::flagsChanged,
                this, &WorkbenchUi::updateControlsVisibilityAnimated);
            updateControlsVisibilityAnimated();
            updateLayoutPanelGeometry();

            if (auto resource = layout->resource())
            {
                const auto oldMargins = display()->viewportMargins(Qn::LayoutMargins);
                const auto margins = resource->data(Qn::LayoutMarginsRole).value<QMargins>();

                if (margins != oldMargins)
                {
                    display()->setViewportMargins(margins, Qn::LayoutMargins);
                    display()->fitInView(true);
                }
            }

        });

    connect(qnRuntime, &QnClientRuntimeSettings::valueChanged, this,
        [this](int id)
        {
           if (id == QnClientRuntimeSettings::VIDEO_WALL_WITH_TIMELINE)
               updateControlsVisibility(false);
        });
}

WorkbenchUi::~WorkbenchUi()
{
    debugLabel = nullptr;

    /* The disconnect call is needed so that our methods don't get triggered while
     * the ui machinery is shutting down. */
    disconnectAll();

    delete m_timeline;
    delete m_calendar;
    delete m_notifications;
    delete m_title;
    delete m_layoutPanel;
    delete m_tree;
    delete m_debugOverlayLabel;

    delete m_controlsWidget;
}

void WorkbenchUi::storeSettings()
{
    QnPaneSettingsMap settings;
    QnPaneSettings& title = settings[Qn::WorkbenchPane::Title];
    title.state = makePaneState(isTitleOpened());

    if (m_tree)
    {
        QnPaneSettings& tree = settings[Qn::WorkbenchPane::Tree];
        tree.state = makePaneState(isTreeOpened(), isTreePinned());
        tree.span = m_tree->item->geometry().width();
    }

    QnPaneSettings& notifications = settings[Qn::WorkbenchPane::Notifications];
    notifications.state = makePaneState(isNotificationsOpened(), isNotificationsPinned());

    QnPaneSettings& navigation = settings[Qn::WorkbenchPane::Navigation];
    navigation.state = makePaneState(isTimelineOpened(), true);

    QnPaneSettings& calendar = settings[Qn::WorkbenchPane::Calendar];
    calendar.state = makePaneState(isCalendarOpened(), isCalendarPinned());

    QnPaneSettings& thumbnails = settings[Qn::WorkbenchPane::Thumbnails];
    thumbnails.state = makePaneState(m_timeline->isThumbnailsVisible());
    thumbnails.span = m_timeline->lastThumbnailsHeight;

    qnSettings->setPaneStateSettings(settings);
    qnSettings->save();
}

void WorkbenchUi::updateCursor()
{
#ifndef Q_OS_MACX
    bool curtained = m_inactive;
    if (display()->view())
        display()->view()->viewport()->setCursor(QCursor(curtained ? Qt::BlankCursor : Qt::ArrowCursor));
#endif
}

bool WorkbenchUi::calculateTimelineVisible(QnResourceWidget* widget) const
{
    if (!widget)
        return false;

    const auto resource = widget->resource();
    if (!resource)
        return false;

    const auto flags = resource->flags();

    /* Any of the flags is sufficient. */
    if (flags & (Qn::still_image | Qn::server | Qn::videowall | Qn::layout))
        return false;

    /* Qn::web_page is as flag combination. */
    if (flags.testFlag(Qn::web_page))
        return false;

    if (flags.testFlag(Qn::desktop_camera))
        return false;

    return accessController()->hasGlobalPermission(GlobalPermission::viewArchive)
        || !flags.testFlag(Qn::live);   /*< Show slider for local files. */
}

action::ActionScope WorkbenchUi::currentScope() const
{
    QGraphicsItem *focusItem = display()->scene()->focusItem();
    if (m_tree && focusItem == m_tree->item)
        return action::TreeScope;

    if (focusItem == m_timeline->item)
        return action::TimelineScope;

    /* We should not handle any button as an action while the item was focused. */
    if (dynamic_cast<QnGraphicsWebView*>(focusItem))
        return action::InvalidScope;

    if (display()->scene()->hasFocus())
        return action::SceneScope;

    return action::MainScope;
}

action::Parameters WorkbenchUi::currentParameters(action::ActionScope scope) const
{
    /* Get items. */
    switch (scope)
    {
        case action::TreeScope:
            return m_tree ? m_tree->widget->currentParameters(scope) : action::Parameters();
        case action::TimelineScope:
            return navigator()->currentWidget();
        case action::SceneScope:
        {
            auto selectedItems = display()->scene()->selectedItems();
            if (selectedItems.empty())
            {
                auto focused = dynamic_cast<QnResourceWidget*>(display()->scene()->focusItem());
                if (focused)
                    selectedItems.append(focused);
            }
            return action::ParameterTypes::widgets(selectedItems);
        }
        default:
            break;
    }
    return action::Parameters();
}


WorkbenchUi::Flags WorkbenchUi::flags() const
{
    return m_flags;
}

void WorkbenchUi::updateControlsVisibility(bool animate)
{    // TODO
    ensureAnimationAllowed(animate);

    const auto layout = workbench()->currentLayout();
    const bool allowedByLayout = (layout
        ? !layout->flags().testFlag(QnLayoutFlag::NoTimeline)
        : true);
    const bool timelineVisible =
        (allowedByLayout && calculateTimelineVisible(navigator()->currentWidget()));

    if (action(action::ToggleLayoutTourModeAction)->isChecked())
    {
        setTimelineVisible(false, animate);
        setTreeVisible(false, animate);
        setTitleVisible(false, animate);
        setNotificationsVisible(false, animate);
        return;
    }

    if (qnRuntime->isVideoWallMode())
    {
        if (qnRuntime->videoWallWithTimeline())
            setTimelineVisible(timelineVisible, animate);
        else
            setTimelineVisible(false, false);
        setTreeVisible(false, false);
        setTitleVisible(false, false);
        setNotificationsVisible(false, false);
        return;
    }

    if (m_inactive)
    {
        bool hovered = isHovered();
        setTimelineVisible(timelineVisible && hovered, animate);
        setTreeVisible(hovered, animate);
        setTitleVisible(hovered, animate);
        setNotificationsVisible(hovered, animate);
    }
    else
    {
        setTimelineVisible(timelineVisible, false);
        setTreeVisible(true, animate);
        setTitleVisible(true, animate);
        setNotificationsVisible(true, animate);
    }

    updateCalendarVisibility(animate);
}

void WorkbenchUi::updateCalendarVisibilityAnimated()
{
    updateCalendarVisibility(true);
}

void WorkbenchUi::updateControlsVisibilityAnimated()
{
    updateControlsVisibility(true);
}

QMargins WorkbenchUi::calculateViewportMargins(
    const QRectF& treeGeometry,
    const QRectF& titleGeometry,
    const QRectF& timelineGeometry,
    const QRectF& notificationsGeometry,
    const QRectF& layoutPanelGeometry)
{
    using namespace std;
    QMargins result;
    if (treeGeometry.isValid())
        result.setLeft(max(0.0, floor(treeGeometry.left() + treeGeometry.width())));

    const auto bottom = floor(layoutPanelGeometry.isValid()
        ? layoutPanelGeometry.bottom()
        : titleGeometry.bottom());
    result.setTop(max(0.0, bottom));

    if (notificationsGeometry.isValid())
        result.setRight(max(0.0, floor(m_controlsWidgetRect.right() - notificationsGeometry.left())));

    if (timelineGeometry.isValid())
        result.setBottom(max(0.0, floor(m_controlsWidgetRect.bottom() - timelineGeometry.top())));

    if (result.left() + result.right() >= m_controlsWidgetRect.width())
    {
        result.setLeft(0.0);
        result.setRight(0.0);
    }

    return result;
}

void WorkbenchUi::setFlags(Flags flags)
{
    if (flags == m_flags)
        return;

    m_flags = flags;

    updateActivityInstrumentState();
    updateViewportMargins(false);
}

bool WorkbenchUi::isTitleUsed() const
{
    return m_title && m_title->isUsed();
}

void WorkbenchUi::updateViewportMargins(bool animate)
{
    using boost::algorithm::any_of;

    auto panelEffectiveGeometry = [](AbstractWorkbenchPanel* panel)
        {
            if (panel && panel->isVisible() && panel->isPinned())
                return panel->effectiveGeometry();
            return QRectF();
        };

    const auto layout = workbench()->currentLayout();
    const bool allowedByLayout = (layout
        ? !layout->flags().testFlag(QnLayoutFlag::NoTimeline)
        : true);

    const bool timelineCanBeVisible = m_timeline && allowedByLayout && any_of(display()->widgets(),
        [this](QnResourceWidget* widget)
        {
            return calculateTimelineVisible(widget);
        });

    auto timelineEffectiveGeometry = (m_timeline && timelineCanBeVisible)
        ? m_timeline->effectiveGeometry()
        : QRectF();

    auto oldMargins = display()->viewportMargins(Qn::PanelMargins);

    QMargins newMargins(0, 0, 0, 0);
    if (m_flags.testFlag(AdjustMargins))
    {
        newMargins = calculateViewportMargins(
            panelEffectiveGeometry(m_tree),
            panelEffectiveGeometry(m_title),
            timelineEffectiveGeometry,
            panelEffectiveGeometry(m_notifications),
            panelEffectiveGeometry(m_layoutPanel)
        );
    }

    if (oldMargins == newMargins)
        return;

    display()->setViewportMargins(newMargins, Qn::PanelMargins);
    display()->fitInView(animate);
}

void WorkbenchUi::updateViewportMarginsAnimated()
{
    updateViewportMargins(true);
}

void WorkbenchUi::updateActivityInstrumentState()
{
    bool zoomed = m_widgetByRole[Qn::ZoomedRole] != nullptr;
    m_controlsActivityInstrument->setEnabled(m_flags & (zoomed ? HideWhenZoomed : HideWhenNormal));
}

bool WorkbenchUi::isHovered() const
{
    return
        (m_timeline->isHovered())
        || (m_tree && m_tree->isHovered())
        || (m_title && m_title->isHovered())
        || (m_notifications && m_notifications->isHovered())
        || (m_calendar && m_calendar->isHovered());
}

WorkbenchUi::Panels WorkbenchUi::openedPanels() const
{
    return
        (isTreeOpened() ? TreePanel : NoPanel)
        | (isTitleOpened() ? TitlePanel : NoPanel)
        | (isTimelineOpened() ? TimelinePanel : NoPanel)
        | (isNotificationsOpened() ? NotificationsPanel : NoPanel)
        ;
}

void WorkbenchUi::setOpenedPanels(Panels panels, bool animate)
{
    ensureAnimationAllowed(animate);

    setTreeOpened(panels & TreePanel, animate);
    setTitleOpened(panels & TitlePanel, animate);
    setTimelineOpened(panels & TimelinePanel, animate);
    setNotificationsOpened(panels & NotificationsPanel, animate);
}

void WorkbenchUi::initGraphicsMessageBoxHolder()
{
    auto overlayWidget = new QnGuiElementsWidget();
    overlayWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(overlayWidget);
    display()->setLayer(overlayWidget, QnWorkbenchDisplay::MessageBoxLayer);

    QGraphicsLinearLayout* messageBoxVLayout = new QGraphicsLinearLayout(Qt::Vertical);
    messageBoxVLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    messageBoxVLayout->setSpacing(0.0);

    QGraphicsLinearLayout* messageBoxHLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    messageBoxHLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    messageBoxHLayout->setSpacing(0.0);

    overlayWidget->setLayout(messageBoxHLayout);

    messageBoxHLayout->addStretch();
    messageBoxHLayout->addItem(messageBoxVLayout);
    messageBoxHLayout->addStretch();

    messageBoxVLayout->addStretch();
    messageBoxVLayout->addItem(new QnGraphicsMessageBoxHolder(overlayWidget));
    messageBoxVLayout->addStretch();
}

void WorkbenchUi::tick(int deltaMSecs)
{
    if (!m_timeline->zoomingIn && !m_timeline->zoomingOut)
        return;

    if (!display()->scene())
        return;

    auto slider = m_timeline->item->timeSlider();

    QPointF pos;
    if (slider->windowStart() <= slider->sliderTimePosition() && slider->sliderTimePosition() <= slider->windowEnd())
        pos = slider->positionFromTime(slider->sliderTimePosition(), true);
    else
        pos = slider->rect().center();

    QGraphicsSceneWheelEvent event(QEvent::GraphicsSceneWheel);
    event.setDelta(360 * 8 * (m_timeline->zoomingIn ? deltaMSecs : -deltaMSecs) / 1000); /* 360 degrees per sec, x8 since delta is measured in in eighths (1/8s) of a degree. */
    event.setPos(pos);
    event.setScenePos(slider->mapToScene(pos));
    display()->scene()->sendEvent(slider, &event);
}

void WorkbenchUi::at_freespaceAction_triggered()
{
    NX_ASSERT(!qnRuntime->isAcsMode(), "This function must not be called in ActiveX mode.");
    if (qnRuntime->isAcsMode())
        return;

    QAction *fullScreenAction = action(action::EffectiveMaximizeAction);

    bool isFullscreen = fullScreenAction->isChecked();

    if (!m_inFreespace)
    {
        m_inFreespace = isFullscreen
           && (isTreePinned() && !isTreeOpened())
           && !isTitleOpened()
           && (isNotificationsPinned() && !isNotificationsOpened())
           && (isTimelinePinned() && !isTimelineOpened());
    }

    if (!m_inFreespace)
    {
        storeSettings();

        if (!isFullscreen)
            fullScreenAction->setChecked(true);

        setTreeOpened(false, isFullscreen);
        setTitleOpened(false, isFullscreen);
        setTimelineOpened(false, isFullscreen);
        setNotificationsOpened(false, isFullscreen);

        m_inFreespace = true;
    }
    else
    {
        QnPaneSettingsMap settings = qnSettings->paneStateSettings();
        setTreeOpened(settings[Qn::WorkbenchPane::Tree].state == Qn::PaneState::Opened, isFullscreen);
        setTitleOpened(settings[Qn::WorkbenchPane::Title].state == Qn::PaneState::Opened, isFullscreen);
        setTimelineOpened(settings[Qn::WorkbenchPane::Navigation].state == Qn::PaneState::Opened, isFullscreen);
        setNotificationsOpened(settings[Qn::WorkbenchPane::Notifications].state == Qn::PaneState::Opened, isFullscreen);

        m_inFreespace = false;
    }
}

void WorkbenchUi::at_activityStopped()
{
    m_inactive = true;

    const bool animate = display()->animationAllowed();
    updateControlsVisibility(animate);

    foreach(QnResourceWidget *widget, display()->widgets())
    {
        widget->setOption(QnResourceWidget::ActivityPresence, false);
        if (!(widget->options() & QnResourceWidget::DisplayInfo))
            widget->setOverlayVisible(false, animate);
    }
    updateCursor();
}

void WorkbenchUi::at_activityStarted()
{
    m_inactive = false;

    const bool animate = display()->animationAllowed();
    updateControlsVisibility(animate);

    foreach(QnResourceWidget *widget, display()->widgets())
    {
        widget->setOption(QnResourceWidget::ActivityPresence, true);
        if (widget->isInfoVisible()) // TODO: #Elric wrong place?
            widget->setOverlayVisible(true, animate);
    }
    updateCursor();
}

void WorkbenchUi::at_display_widgetChanged(Qn::ItemRole role)
{
    bool alreadyZoomed = m_widgetByRole[Qn::ZoomedRole] != nullptr;

    QnResourceWidget* newWidget = display()->widget(role);
    m_widgetByRole[role] = newWidget;

    /* Update activity listener instrument. */
    if (role == Qn::ZoomedRole)
    {
        updateActivityInstrumentState();
        updateViewportMargins();

        if (newWidget)
        {
            if (!alreadyZoomed)
                m_unzoomedOpenedPanels = openedPanels();
            const auto panels = newWidget->options().testFlag(QnResourceWidget::DisplayMotion)
                ? TimelinePanel
                : NoPanel;
            setOpenedPanels(panels, true);
        }
        else
        {
            /* User may have opened some panels while zoomed,
             * we want to leave them opened even if they were closed before. */
            setOpenedPanels(m_unzoomedOpenedPanels | openedPanels(), true);

            /* Viewport margins have changed, force fit-in-view. */
            display()->fitInView(display()->animationAllowed());
        }

        qreal masterOpacity = newWidget ? kZoomedTimelineOpacity : kOpaque;
        if (m_timeline)
            m_timeline->setMasterOpacity(masterOpacity);
        if (m_calendar)
            m_calendar->setMasterOpacity(masterOpacity);
    }

    if (qnRuntime->isVideoWallMode())
    {
        switch (role)
        {
            case Qn::ZoomedRole:
            case Qn::RaisedRole:
                if (newWidget)
                    updateControlsVisibility(true);
                else
                    setTimelineVisible(false, true);
                break;

            default:
                break;
        }
    }
}

void WorkbenchUi::ensureAnimationAllowed(bool &animate)
{
    if (animate && qnRuntime->lightMode().testFlag(Qn::LightModeNoAnimation))
        animate = false;
}

#pragma region ControlsWidget

void WorkbenchUi::at_controlsWidget_geometryChanged()
{
    QGraphicsWidget *controlsWidget = m_controlsWidget;
    QRectF rect = controlsWidget->rect();
    if (qFuzzyEquals(m_controlsWidgetRect, rect))
        return;

    m_controlsWidgetRect = rect;

    /* We lay everything out manually. */

    if (m_timeline)
        m_timeline->stopAnimations();
    m_timeline->updateGeometry();

    if (m_title)
    {
        m_title->stopAnimations();
        m_title->item->setGeometry(QRectF(
            0.0,
            m_title->item->pos().y(),
            rect.width(),
            m_title->item->size().height()));
    }

    if (m_notifications)
    {
        m_notifications->stopAnimations();
        if (m_notifications->xAnimator->isRunning())
            m_notifications->xAnimator->stop();
        m_notifications->item->setX(rect.right() + (isNotificationsOpened() ? -m_notifications->item->size().width() : 1.0 /* Just in case. */));
    }

    if (m_tree)
        m_tree->stopAnimations();

    if (m_calendar)
        m_calendar->stopAnimations();

    updateTreeGeometry();
    updateNotificationsGeometry();
    updateLayoutPanelGeometry();
    updateFpsGeometry();
    updateViewportMargins(false);
}

void WorkbenchUi::createControlsWidget()
{
    m_controlsWidget = new QnGuiElementsWidget();
    m_controlsWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(m_controlsWidget);
    display()->setLayer(m_controlsWidget, QnWorkbenchDisplay::UiLayer);

    installEventHandler(m_controlsWidget, QEvent::WindowDeactivate, this,
        [this]() { display()->scene()->setActiveWindow(m_controlsWidget); });

    connect(m_controlsWidget, &QGraphicsWidget::geometryChanged, this, &WorkbenchUi::at_controlsWidget_geometryChanged);
}

#pragma endregion Main controls widget methods

#pragma region TreeWidget

void WorkbenchUi::setTreeVisible(bool visible, bool animate)
{
    if (m_tree)
        m_tree->setVisible(visible, animate);
}

bool WorkbenchUi::isTreePinned() const
{
    return m_tree && m_tree->isPinned();
}

bool WorkbenchUi::isTreeVisible() const
{
    return m_tree && m_tree->isVisible();
}

bool WorkbenchUi::isTreeOpened() const
{
    return m_tree && m_tree->isOpened();
}

void WorkbenchUi::setTreeOpened(bool opened, bool animate)
{
    if (!m_tree)
        return;
    m_tree->setOpened(opened, animate);
}

QRectF WorkbenchUi::updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry)
{
    QPointF pos(treeGeometry.x(), qMax(titleGeometry.bottom(), 0.0));
    qreal bottom = isTimelineVisible()
        ? qMin(sliderGeometry.y(), m_controlsWidgetRect.bottom())
        : m_controlsWidgetRect.bottom();

    QSizeF size(treeGeometry.width(), bottom - pos.y());

    return QRectF(pos, size);
}

void WorkbenchUi::updateTreeGeometry()
{
    if (!m_tree)
        return;

    QRectF titleGeometry = (m_title && m_title->isVisible())
        ? m_title->item->geometry()
        : QRectF();

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedTreeGeometry(m_tree->item->geometry(), titleGeometry, m_timeline->item->geometry());
    m_tree->item->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_tree->item->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline->isVisible() && isTreeVisible())
    {
        sliderPos = QPointF(m_timeline->item->pos().x(), m_controlsWidgetRect.bottom());
    }
    else
    {
        sliderPos = m_timeline->effectiveGeometry().topLeft();
        defer |= !qFuzzyEquals(sliderPos, m_timeline->item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }

    /* Calculate title target position. */
    QRectF titleEffectiveGeometry = (m_title && m_title->isVisible())
        ? m_title->effectiveGeometry()
        : QRectF();

    /* If animation is running, then geometry sync should be deferred. */
    defer |= !qFuzzyEquals(titleGeometry, titleEffectiveGeometry);

    /* Calculate target geometry. */
    geometry = updatedTreeGeometry(m_tree->item->geometry(), titleEffectiveGeometry, QRectF(sliderPos, m_timeline->item->size()));
    if (qFuzzyEquals(geometry, m_tree->item->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_tree->item->size().height())
        return;

    m_tree->item->resize(geometry.size());
}

void WorkbenchUi::createTreeWidget(const QnPaneSettings& settings)
{
    m_tree = new ResourceTreeWorkbenchPanel(settings, m_controlsWidget, this);

    connect(m_tree, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened && m_tree->isPinned())
                m_inFreespace = false;
        });
    connect(m_tree, &AbstractWorkbenchPanel::visibleChanged, this,
        &WorkbenchUi::updateTreeGeometry);

    connect(m_tree, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_tree, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_tree, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateViewportMarginsAnimated);
    connect(m_tree, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateLayoutPanelGeometry);
}

#pragma endregion Tree widget methods

#pragma region TitleWidget

bool WorkbenchUi::isTitleVisible() const
{
    return m_title && m_title->isVisible();
}

void WorkbenchUi::setTitleUsed(bool used)
{
    if (!m_title)
        return;

    if (m_title->isUsed() == used)
        return;

    m_title->setUsed(used);
    updateControlsVisibility(false);
}

void WorkbenchUi::setTitleOpened(bool opened, bool animate)
{
    if (m_title)
        m_title->setOpened(opened, animate);
}

void WorkbenchUi::setTitleVisible(bool visible, bool animate)
{
    if (m_title)
        m_title->setVisible(visible, animate);
}

bool WorkbenchUi::isTitleOpened() const
{
    return m_title && m_title->isOpened();
}

void WorkbenchUi::createTitleWidget(const QnPaneSettings& settings)
{
    m_title = new TitleWorkbenchPanel(settings, m_controlsWidget, this);
    connect(m_title, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened)
                m_inFreespace = false;
        });

    connect(m_title, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_title, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_title, &AbstractWorkbenchPanel::visibleChanged, this,
        [this](bool /*value*/, bool animated)
        {
            updateTreeGeometry();
            updateNotificationsGeometry();
            updateLayoutPanelGeometry();
            updateFpsGeometry();
            updateViewportMargins(animated);
        });

    connect(m_title, &AbstractWorkbenchPanel::geometryChanged, this,
        [this]
        {
            updateTreeGeometry();
            updateNotificationsGeometry();
            updateLayoutPanelGeometry();
            updateFpsGeometry();
            updateViewportMargins();
        });
}

void WorkbenchUi::createLayoutPanelWidget(const QnPaneSettings& settings)
{
    m_layoutPanel = new nx::vms::client::desktop::ui::workbench::SpecialLayoutPanel(
        settings, m_controlsWidget, this);

    connect(m_layoutPanel, &AbstractWorkbenchPanel::geometryChanged,
        this, &WorkbenchUi::updateViewportMarginsAnimated);
}

#pragma endregion Title methods

#pragma region NotificationsWidget

bool WorkbenchUi::isNotificationsVisible() const
{
    return m_notifications && m_notifications->isVisible();
}

bool WorkbenchUi::isNotificationsOpened() const
{
    return m_notifications && m_notifications->isOpened();
}

bool WorkbenchUi::isNotificationsPinned() const
{
    return m_notifications && m_notifications->isPinned();
}

void WorkbenchUi::setNotificationsOpened(bool opened, bool animate)
{
    if (m_notifications)
        m_notifications->setOpened(opened, animate);
}

void WorkbenchUi::setNotificationsVisible(bool visible, bool animate)
{
    if (m_notifications)
        m_notifications->setVisible(visible, animate);
}

QRectF WorkbenchUi::updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry)
{
    QPointF pos(notificationsGeometry.x(), qMax(titleGeometry.bottom(), 0.0));

    qreal top = m_controlsWidgetRect.bottom();
    if (m_timeline->isVisible() && m_timeline->isOpened())
        top = m_timeline->effectiveGeometry().top();

    const qreal maxHeight = top - pos.y();

    QSizeF size(notificationsGeometry.width(), maxHeight);
    return QRectF(pos, size);
}

void WorkbenchUi::updateLayoutPanelGeometry()
{
    if (!m_layoutPanel || !m_layoutPanel->widget())
        return;

    const auto titleGeometry = m_title && m_title->isVisible()
        ? m_title->item->geometry()
        : QRectF();

    const auto notificationsLeft = m_notifications && m_notifications->isVisible()
        ? m_notifications->item->geometry().left()
        : m_controlsWidgetRect.right();

    const auto treeRight = m_tree && m_tree->isVisible()
        ? m_tree->item->geometry().right()
        : m_controlsWidgetRect.left();

    const auto topLeft = QPointF(treeRight, titleGeometry.bottom());
    const auto size = QSizeF(notificationsLeft - treeRight, 0); // TODO #ynikitenkov: add height handling
    m_layoutPanel->widget()->setGeometry(QRectF(topLeft, size));
}

void WorkbenchUi::updateNotificationsGeometry()
{
    if (!m_notifications)
        return;

    /* Update painting rect the "fair" way. */
    QRectF titleGeometry = (m_title && m_title->isVisible())
        ? m_title->item->geometry()
        : QRectF();

    QRectF geometry = updatedNotificationsGeometry(
        m_notifications->item->geometry(),
        titleGeometry);

    /* Always change position. */
    m_notifications->item->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline->isVisible() && isNotificationsVisible())
    {
        sliderPos = QPointF(m_timeline->item->pos().x(), m_controlsWidgetRect.bottom());
    }
    else
    {
        sliderPos = m_timeline->effectiveGeometry().topLeft();
        defer |= !qFuzzyEquals(sliderPos, m_timeline->item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }

    /* Calculate title target position. */
    QRectF titleEffectiveGeometry = (m_title && m_title->isVisible())
        ? m_title->effectiveGeometry()
        : QRectF();

    /* If animation is running, then geometry sync should be deferred. */
    defer |= !qFuzzyEquals(titleGeometry, titleEffectiveGeometry);

    /* Calculate target geometry. */
    geometry = updatedNotificationsGeometry(m_notifications->item->geometry(),
        titleEffectiveGeometry);

    if (qFuzzyEquals(geometry, m_notifications->item->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_notifications->item->size().height())
        return;

    m_notifications->item->resize(geometry.size());

    /* All tooltips should fit to rect of maxHeight */
    QRectF tooltipsEnclosingRect(
        m_controlsWidgetRect.left(),
        m_notifications->item->y(),
        m_controlsWidgetRect.width(),
        m_notifications->item->geometry().height());
    m_notifications->tooltipsEnclosingRect = m_controlsWidget->mapRectToItem(
        m_notifications->item, tooltipsEnclosingRect);
}

void WorkbenchUi::createNotificationsWidget(const QnPaneSettings& settings)
{
    m_notifications = new NotificationsWorkbenchPanel(settings, m_controlsWidget, this);
    connect(m_notifications, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened && m_notifications->isPinned())
                m_inFreespace = false;
        });
    connect(m_notifications, &AbstractWorkbenchPanel::visibleChanged, this,
        &WorkbenchUi::updateNotificationsGeometry);
    connect(m_notifications->item, &QGraphicsWidget::widthChanged, this,
        &WorkbenchUi::updateNotificationsGeometry);

    connect(m_notifications, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_notifications, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_notifications, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateViewportMarginsAnimated);
    connect(m_notifications, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateFpsGeometry);
    connect(m_notifications, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateLayoutPanelGeometry);
}

#pragma endregion Notifications widget methods

#pragma region CalendarWidget

bool WorkbenchUi::isCalendarPinned() const
{
    return m_calendar && m_calendar->isPinned();
}

bool WorkbenchUi::isCalendarOpened() const
{
    return m_calendar && m_calendar->isOpened();
}

bool WorkbenchUi::isCalendarVisible() const
{
    return m_calendar && m_calendar->isVisible();
}

void WorkbenchUi::setCalendarOpened(bool opened, bool animate)
{
    if (m_calendar)
        m_calendar->setOpened(opened, animate);
}

void WorkbenchUi::updateCalendarVisibility(bool animate)
{
    if (!m_calendar)
        return;

    /* Small hack. We have a signal that updates visibility if a calendar receive new data */
    bool calendarEmpty = navigator()->calendar()->isEmpty();
    // TODO: #GDM refactor to the same logic as timeline

    bool calendarEnabled = !calendarEmpty
        && navigator()->currentWidget()
        && navigator()->currentWidget()->resource()->flags().testFlag(Qn::utc)
        && (!m_inactive || m_timeline->isHovered())
        && m_timeline->isVisible()
        && m_timeline->isOpened();

    /* Avoid double notifications geometry calculations. */
    if (m_calendar->isEnabled() == calendarEnabled)
        return;

    m_calendar->setEnabled(calendarEnabled, animate);
    updateNotificationsGeometry();
}

void WorkbenchUi::updateCalendarGeometry()
{
    QRectF geometry = m_calendar->item->geometry();
    geometry.moveRight(m_controlsWidgetRect.right());
    geometry.moveBottom(m_timeline->effectiveGeometry().top());
    m_calendar->setOrigin(geometry.topLeft());
}

void WorkbenchUi::createCalendarWidget(const QnPaneSettings& settings)
{
    m_calendar = new CalendarWorkbenchPanel(settings, m_controlsWidget, this);

    // TODO: #GDM refactor indirect dependency
    connect(navigator()->calendar(), &QnCalendarWidget::emptyChanged, this,
        &WorkbenchUi::updateCalendarVisibilityAnimated);

    connect(m_calendar, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_calendar, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
}

#pragma endregion Calendar and DayTime widget methods

#pragma region TimelineWidget

bool WorkbenchUi::isTimelineVisible() const
{
    return m_timeline->isVisible();
}

bool WorkbenchUi::isTimelineOpened() const
{
    return m_timeline->isOpened();
}

bool WorkbenchUi::isTimelinePinned() const
{
    /* Auto-hide slider when some item is zoomed, otherwise - don't. */
    return m_timeline->isPinned() ||
        m_widgetByRole[Qn::ZoomedRole] == nullptr;
}

void WorkbenchUi::setTimelineOpened(bool opened, bool animate)
{
    if (qnRuntime->isVideoWallMode())
        opened = true;

    m_timeline->setOpened(opened, animate);
}

void WorkbenchUi::setTimelineVisible(bool visible, bool animate)
{
    m_timeline->setVisible(visible, animate);
}

void WorkbenchUi::createTimelineWidget(const QnPaneSettings& settings)
{
    m_timeline = new TimelineWorkbenchPanel(settings, m_controlsWidget, this);

    connect(m_timeline, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened, bool animated)
        {
            if (opened && m_timeline->isPinned())
                m_inFreespace = false;
            updateCalendarVisibility(animated);
        });

    connect(m_timeline, &AbstractWorkbenchPanel::visibleChanged, this,
        [this](bool /*value*/, bool animated)
        {
            updateTreeGeometry();
            updateNotificationsGeometry();
            updateCalendarVisibility(animated);
            updateViewportMargins(animated);
        });

    connect(m_timeline, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_timeline, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_timeline, &AbstractWorkbenchPanel::geometryChanged, this,
        [this]
        {
            updateTreeGeometry();
            updateNotificationsGeometry();
            updateViewportMargins();
            updateCalendarGeometry();
        });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            /// If tour mode is going to be turned on, focus should be forced to main window
            /// because otherwise we can't cancel tour mode by clicking any key (in some cases)
            if (toggled)
                mainWindowWidget()->setFocus();
        });

    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        [this](){ updateControlsVisibility(false); });
}

#pragma endregion Timeline methods

#pragma region DebugWidget

void WorkbenchUi::createDebugWidget()
{
    m_debugOverlayLabel = new QnDebugProxyLabel(m_controlsWidget);
    m_debugOverlayLabel->setAcceptedMouseButtons(Qt::NoButton);
    m_debugOverlayLabel->setAcceptHoverEvents(false);
    m_debugOverlayLabel->setMessagesLimit(40);
    setPaletteColor(m_debugOverlayLabel, QPalette::Window, QColor(127, 127, 127, 60));
    setPaletteColor(m_debugOverlayLabel, QPalette::WindowText, QColor(63, 255, 216));
    auto updateDebugGeometry =
        [&]()
        {
            QRectF titleEffectiveGeometry = (m_title && m_title->isVisible())
            ? m_title->effectiveGeometry()
            : QRectF();

            QPointF pos = QPointF(titleEffectiveGeometry.bottomLeft());
            if (qFuzzyEquals(pos, m_debugOverlayLabel->pos()))
                return;

            m_debugOverlayLabel->setPos(pos);
        };
    m_debugOverlayLabel->setVisible(true);

    if (m_title)
    {
        connect(m_title, &AbstractWorkbenchPanel::geometryChanged, this, updateDebugGeometry);
        debugLabel = m_debugOverlayLabel;
        previousMsgHandler = qInstallMessageHandler(uiMsgHandler);
    }
    updateDebugGeometry();

    display()->view()->addAction(action(action::ShowDebugOverlayAction));
    connect(action(action::ShowDebugOverlayAction), &QAction::toggled, this,
        [&](bool toggled) { m_debugOverlayLabel->setVisible(toggled); });
}

#pragma endregion Debug overlay methods

#pragma region FpsWidget

bool WorkbenchUi::isFpsVisible() const
{
    return m_fpsItem->isVisible();
}

void WorkbenchUi::setFpsVisible(bool fpsVisible)
{
    if (fpsVisible == isFpsVisible())
        return;

    m_fpsItem->setVisible(fpsVisible);
    m_fpsItem->setText(QString());

    action(action::ShowFpsAction)->setChecked(fpsVisible);
}

void WorkbenchUi::updateFpsGeometry()
{
    qreal right = m_notifications
        ? m_notifications->backgroundItem->geometry().left()
        : m_controlsWidgetRect.right();

    QRectF titleEffectiveGeometry = (m_title && m_title->isVisible())
        ? m_title->effectiveGeometry()
        : QRectF();

    QPointF pos = QPointF(
        right - m_fpsItem->size().width(),
        titleEffectiveGeometry.bottom());

    if (qFuzzyEquals(pos, m_fpsItem->pos()))
        return;

    m_fpsItem->setPos(pos);
}

void WorkbenchUi::createFpsWidget()
{
    m_fpsItem = new QnProxyLabel(m_controlsWidget);
    m_fpsItem->setAcceptedMouseButtons(0);
    m_fpsItem->setAcceptHoverEvents(false);
    m_fpsItem->setText(lit("...."));
    updateFpsGeometry();
    setPaletteColor(m_fpsItem, QPalette::Window, Qt::transparent);
    setPaletteColor(m_fpsItem, QPalette::WindowText, QColor(63, 159, 216));
    display()->setLayer(m_fpsItem, QnWorkbenchDisplay::MessageBoxLayer);

    connect(action(action::ShowFpsAction), &QAction::toggled, this, &WorkbenchUi::setFpsVisible);
    connect(m_fpsItem, &QGraphicsWidget::geometryChanged, this, &WorkbenchUi::updateFpsGeometry);
    setFpsVisible(qnRuntime->isProfilerMode());
}

#pragma endregion Fps widget methods

} // namespace nx::vms::client::desktop
