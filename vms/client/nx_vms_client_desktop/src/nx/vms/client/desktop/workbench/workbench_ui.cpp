// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_ui.h"

#include <algorithm>
#include <cmath> /* For std::floor. */

#include <QtCore/QMargins>
#include <QtGui/QAction>
#include <QtWidgets/QGraphicsLayout>

#include <qt_graphics_items/graphics_widget.h>

#include <camera/fps_calculator.h>
#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/trace/trace.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/instruments/frame_time_points_provider_instrument.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/left_panel/left_panel_widget.h>
#include <nx/vms/client/desktop/left_panel/qml_resource_browser_widget.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameter_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/session_manager/session_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/scene/widgets/performance_info_widget.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/ui/scene/widgets/timeline_calendar_widget.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/viewport_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/gui_elements_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_widget.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/event_processors.h>

#include "panels/calendar_workbench_panel.h"
#include "panels/left_workbench_panel.h"
#include "panels/notifications_workbench_panel.h"
#include "panels/resource_tree_workbench_panel.h"
#include "panels/special_layout_panel.h"
#include "panels/timeline_workbench_panel.h"
#include "panels/title_workbench_panel.h"
#include "workbench.h"

namespace nx::vms::client::desktop {

using namespace ui;

namespace {

Qn::PaneState makePaneState(bool opened, bool pinned = true)
{
    return pinned ? (opened ? Qn::PaneState::Opened : Qn::PaneState::Closed) : Qn::PaneState::Unpinned;
}

constexpr int kHideControlsTimeoutMs = 2000;

constexpr qreal kZoomedTimelineOpacity = 0.9;

static const QString kWorkbenchUiDataKey = "workbenchSettings";

static constexpr int kReservedSceneWidth = 224;

int calculateAutoFpsLimit(QnWorkbenchLayout* layout)
{
    static constexpr auto kIdleFps = 15;
    static constexpr auto kUnlimitedFps = 0;

    if (!appContext()->localSettings()->autoFpsLimit())
        return kUnlimitedFps;

    if (!layout)
        return kIdleFps;

    const auto& items = layout->items();

    if (items.count() == 0) // Empty layout.
        return kIdleFps;

    QnVirtualCameraResourceList cameras;

    // Gather cameras placed on current layout.
    for (const auto item: items)
    {
        if (auto camera = item->resource().dynamicCast<QnVirtualCameraResource>())
            cameras.push_back(camera);
        else
            return kUnlimitedFps; //< Item is not camera - do not limit fps.
    }

    int result = Qn::calculateMaxFps(cameras);

    return result == std::numeric_limits<int>::max()
        ? kUnlimitedFps
        : result;
}

DelegateState serializeState(const QnPaneSettingsMap& settings)
{
    DelegateState result;

    for (auto it = settings.begin(); it != settings.end(); ++it)
    {
        const auto key = QString::fromStdString(nx::reflect::toString(it.key()));
        QJson::serialize(it.value(), &result[key]);
    }

    return result;
}

} // namespace

class WorkbenchUi::StateDelegate: public ClientStateDelegate
{
public:
    StateDelegate(WorkbenchUi* ui): m_ui(ui)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& /*params*/) override
    {
        if (!flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
            return false;

        bool success = true;
        QnPaneSettingsMap settings;

        for (auto it = state.begin(); it != state.end(); ++it)
        {
            bool parseOk = false;
            auto pane = nx::reflect::fromString<Qn::WorkbenchPane>(it.key().toStdString(), &parseOk);
            QnPaneSettings paneSettings;

            if (parseOk && QJson::deserialize(it.value(), &paneSettings))
            {
                settings[pane] = paneSettings;
            }
            else
            {
                success = false;
            }
        }
        m_ui->m_settings = settings;
        m_ui->loadSettings(false, false);

        reportStatistics("left_panel_visible", m_ui->isLeftPanelOpened());
        reportStatistics("notifications_panel_visible", m_ui->isNotificationsOpened());
        reportStatistics("timeline_visible", m_ui->isTimelineOpened());
        reportStatistics("timeline_thumbnails_visible", m_ui->m_timeline->isThumbnailsVisible());
        reportStatistics("timeline_thumbnails_height", m_ui->m_timeline->thumbnailsHeight());
        // reportStatistics("timeline_sound_level", ...);

        return success;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
        {
            m_ui->storeSettings();
            *state = serializeState(m_ui->m_settings);
        }
    }

    virtual DelegateState defaultState() const override
    {
        return serializeState(Qn::defaultPaneSettings());
    }

private:
    WorkbenchUi* m_ui;
};

static QtMessageHandler previousMsgHandler = nullptr;
static QnDebugProxyLabel* debugLabel = nullptr;

static void uiMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (previousMsgHandler)
        previousMsgHandler(type, ctx, msg);

    if (!debugLabel)
        return;

    debugLabel->appendTextQueued(msg);
}

WorkbenchUi::WorkbenchUi(WindowContext* windowContext, QObject *parent):
    base_type(parent),
    WindowContextAware(windowContext),
    m_instrumentManager(display()->instrumentManager())
{
    QGraphicsLayout::setInstantInvalidatePropagation(true);

    /* Install and configure instruments. */

    m_controlsActivityInstrument = new ActivityListenerInstrument(true, kHideControlsTimeoutMs, this);

    m_instrumentManager->installInstrument(m_controlsActivityInstrument);

    m_connections << connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityStopped, this,
        &WorkbenchUi::at_activityStopped);
    m_connections << connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityResumed, this,
        &WorkbenchUi::at_activityStarted);

    /* Create controls. */
    createControlsWidget();

    /* Animation. */
    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &WorkbenchUi::tick);
    m_animationTimerListener->setTimer(m_instrumentManager->animationTimer());
    m_animationTimerListener->startListening();

    /* Fps counter. */
    createFpsWidget();

    if (qnRuntime->isDesktopMode())
    {
        // TODO: Should remake this items.
        createLeftPanelWidget(m_settings[Qn::WorkbenchPane::Tree]); //< Tree panel.
        createTitleWidget(m_settings[Qn::WorkbenchPane::Title]); //< Title bar.
        createLayoutPanelWidget(m_settings[Qn::WorkbenchPane::SpecialLayout]); //< Special layout
        if (!qnRuntime->lightMode().testFlag(Qn::LightModeNoNotifications))
            createNotificationsWidget(m_settings[Qn::WorkbenchPane::Notifications]); //< Notifications
    }

    /* Calendar. */
    createCalendarWidget(m_settings[Qn::WorkbenchPane::Calendar]);

    /* Navigation slider. */
    createTimelineWidget(m_settings[Qn::WorkbenchPane::Navigation]);

    /* Make timeline panel aware of calendar panel. */
    m_timeline->setCalendarPanel(m_calendar);

    loadSettings(false, true);

    /* Windowed title shadow. */
    auto windowedTitleShadow = new QnEdgeShadowWidget(m_controlsWidget,
        m_controlsWidget, Qt::TopEdge, kShadowThickness, QnEdgeShadowWidget::kInner);
    windowedTitleShadow->setZValue(ShadowItemZOrder);

    /* Debug overlay */
    if (ini().developerMode && ini().developerGuiPanel)
        createDebugWidget();

    new nx::vms::client::desktop::SceneBanners(mainWindow()->view()); //< Owned by the view.

    // This overlay is kept for compatibility with some old features.
    // For example its removal causes some graphics scene item cursors not working.
    auto guiElementsWidget = new QnGuiElementsWidget();
    guiElementsWidget->setObjectName("OverlayWidgetLayer");
    guiElementsWidget->setAcceptedMouseButtons(Qt::NoButton);
    mainWindow()->scene()->addItem(guiElementsWidget);
    display()->setLayer(guiElementsWidget, QnWorkbenchDisplay::MessageBoxLayer);

    /* Connect to display. */
    mainWindow()->view()->addAction(action(menu::FreespaceAction));
    m_connections << connect(display(), &QnWorkbenchDisplay::widgetChanged, this,
        &WorkbenchUi::at_display_widgetChanged);

    /* After widget was removed we can possibly increase margins. */
    m_connections << connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
        &WorkbenchUi::updateViewportMarginsAnimated, Qt::QueuedConnection);

    m_connections << connect(action(menu::FreespaceAction), &QAction::triggered, this,
        &WorkbenchUi::at_freespaceAction_triggered);
    m_connections << connect(action(menu::FullscreenResourceAction), &QAction::triggered, this,
        &WorkbenchUi::at_fullscreenResourceAction_triggered);
    m_connections << connect(action(menu::EffectiveMaximizeAction), &QAction::triggered, this,
        [this, windowedTitleShadow]()
        {
            if (m_inFreespace)
                at_freespaceAction_triggered();

            windowedTitleShadow->setVisible(
                !action(menu::EffectiveMaximizeAction)->isChecked());
        });

    /* Init fields. */
    setFlags(HideWhenNormal | HideWhenZoomed | AdjustMargins);

    setTimelineVisible(false, false);
    setLeftPanelVisible(true, false);
    setTitleVisible(false, false);
    setNotificationsVisible(false, false);
    updateControlsVisibility(false);

    // TODO: #ynikitenkov before final commit: add currentLayoutFlags to the workbench()?
    // Possibly it should be used in several places
    m_connections << connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        [this]()
        {
            const auto layout = workbench()->currentLayout();
            if (!layout)
                return;

            disconnect(layout, &QnWorkbenchLayout::flagsChanged,
                this, &WorkbenchUi::updateControlsVisibilityAnimated);
        });

    m_connections << connect(workbench(), &Workbench::currentLayoutChanged, this,
        [this]()
        {
            const auto layout = workbench()->currentLayout();
            if (!layout)
                return;

            m_connections << connect(layout, &QnWorkbenchLayout::flagsChanged,
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

    m_connections << connect(action(menu::ShowTimeLineOnVideowallAction), &QAction::triggered,
        this, [this] { updateControlsVisibility(false); });

    appContext()->clientStateHandler()->registerDelegate(
        kWorkbenchUiDataKey, std::make_unique<StateDelegate>(this));
    //TODO: #spanasenko Save state occasionally?

    m_connections << connect(&appContext()->localSettings()->autoFpsLimit,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &WorkbenchUi::updateAutoFpsLimit);

    updateAutoFpsLimit();

    m_connections << connect(workbench(),
        &Workbench::currentLayoutItemsChanged,
        this,
        &WorkbenchUi::updateAutoFpsLimit);

    m_connections << connect(action(menu::SearchResourcesAction), &QAction::triggered, this,
        [this]()
        {
            setLeftPanelVisible(/*visible*/ true, /*animate*/ false);
            setLeftPanelOpened(/*opened*/ true, /*animate*/ true);
        });
}

void WorkbenchUi::updateAutoFpsLimit()
{
    NX_TRACE_COUNTER("Items").args({{
        "items",
        workbench()->currentLayout()->items().count()
    }});

    m_instrumentManager->setFpsLimit(calculateAutoFpsLimit(workbench()->currentLayout()));
}

WorkbenchUi::~WorkbenchUi()
{
    debugLabel = nullptr;

    appContext()->clientStateHandler()->unregisterDelegate(kWorkbenchUiDataKey);

    delete m_timeline;
    delete m_calendar;
    delete m_notifications;
    delete m_title;
    delete m_layoutPanel;
    delete m_leftPanel;
    delete m_debugOverlayLabel;

    delete m_controlsWidget;
}

void WorkbenchUi::storeSettings()
{
    m_settings.clear();

    QnPaneSettings& title = m_settings[Qn::WorkbenchPane::Title];
    title.state = makePaneState(isTitleOpened());
    title.expanded = m_title->isExpanded();

    if (m_leftPanel)
    {
        QnPaneSettings& left = m_settings[Qn::WorkbenchPane::Tree];
        left.state = makePaneState(isLeftPanelOpened(), isLeftPanelPinned());
        if (!ini().newPanelsLayout)
            left.span = m_leftPanel->geometry().width();
    }

    if (m_notifications)
    {
        QnPaneSettings& notifications = m_settings[Qn::WorkbenchPane::Notifications];
        notifications.state = makePaneState(isNotificationsOpened(), isNotificationsPinned());
        if (!ini().newPanelsLayout)
            notifications.span = m_notifications->geometry().width();
    }

    QnPaneSettings& navigation = m_settings[Qn::WorkbenchPane::Navigation];
    navigation.state = makePaneState(isTimelineOpened(), true);

    QnPaneSettings& calendar = m_settings[Qn::WorkbenchPane::Calendar];
    calendar.state = makePaneState(isCalendarOpened());

    QnPaneSettings& thumbnails = m_settings[Qn::WorkbenchPane::Thumbnails];
    thumbnails.state = makePaneState(m_timeline->isThumbnailsVisible());
    thumbnails.span = m_timeline->thumbnailsHeight();
}

void WorkbenchUi::updateCursor()
{
#ifndef Q_OS_MACX
    bool curtained = m_inactive;
    if (mainWindow()->view() && !m_timeline->isHovered())
        mainWindow()->view()->viewport()->setCursor(QCursor(curtained ? Qt::BlankCursor : Qt::ArrowCursor));
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

    if (const auto accessController = ResourceAccessManager::accessController(resource))
    {
        return accessController->hasPermissions(resource, Qn::ViewFootagePermission)
            || !flags.testFlag(Qn::live); //< Show slider for local files.
    }

    return false;
}

menu::ActionScope WorkbenchUi::currentScope() const
{
    if (m_leftPanel && m_leftPanel->isFocused())
    {
        if (qobject_cast<ResourceTreeWorkbenchPanel*>(m_leftPanel))
            return menu::TreeScope;

        if (auto lp = qobject_cast<LeftWorkbenchPanel*>(m_leftPanel))
            return lp->currentScope();
    }

    QGraphicsItem *focusItem = mainWindow()->scene()->focusItem();
    if (focusItem == m_timeline->item)
        return menu::TimelineScope;

    /* We should not handle any button as an action while the item was focused. */
    if (dynamic_cast<GraphicsWebEngineView*>(focusItem))
        return menu::InvalidScope;

    if (mainWindow()->scene()->hasFocus())
        return menu::SceneScope;

    return menu::MainScope;
}

menu::Parameters WorkbenchUi::currentParameters(menu::ActionScope scope) const
{
    /* Get items. */
    switch (scope)
    {
        case menu::TreeScope:
        {
            if (auto tree = qobject_cast<ResourceTreeWorkbenchPanel*>(m_leftPanel))
                return tree->widget->currentParameters(scope);
            if (auto lp = qobject_cast<LeftWorkbenchPanel*>(m_leftPanel))
                return lp->currentParameters(scope);
        }

        case menu::TimelineScope:
            return navigator()->currentWidget();

        case menu::SceneScope:
        {
            auto selectedItems = mainWindow()->scene()->selectedItems();
            if (selectedItems.empty())
            {
                auto focused = dynamic_cast<QnResourceWidget*>(mainWindow()->scene()->focusItem());
                if (focused)
                    selectedItems.append(focused);
            }
            return menu::ParameterTypes::widgets(selectedItems);
        }

        default:
            break;
    }
    return menu::Parameters();
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

    if (action(menu::ToggleShowreelModeAction)->isChecked())
    {
        setTimelineVisible(false, animate);
        setLeftPanelVisible(false, animate);
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
        setLeftPanelVisible(false, false);
        setTitleVisible(false, false);
        setNotificationsVisible(false, false);
        return;
    }

    const bool notificationsAllowed = !system()->user().isNull();

    if (m_inactive)
    {
        bool hovered = isHovered();
        setTimelineVisible(timelineVisible && hovered, animate);
        setLeftPanelVisible(hovered, animate);
        setTitleVisible(hovered && m_titleIsUsed, animate);
        setNotificationsVisible(notificationsAllowed && hovered, animate);
    }
    else
    {
        setTimelineVisible(timelineVisible, false);
        setLeftPanelVisible(true, animate);
        setTitleVisible(true && m_titleIsUsed, animate);
        setNotificationsVisible(notificationsAllowed, animate);
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

void WorkbenchUi::updateViewportMargins(bool animate)
{
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

    const auto& widgets = display()->widgets();
    const bool timelineCanBeVisible = m_timeline && allowedByLayout && std::any_of(
        widgets.begin(), widgets.end(),
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
            panelEffectiveGeometry(m_leftPanel),
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
        || (m_leftPanel && m_leftPanel->isHovered())
        || (m_title && m_title->isHovered())
        || (m_notifications && m_notifications->isHovered())
        || (m_calendar && m_calendar->isHovered());
}

WorkbenchUi::Panels WorkbenchUi::openedPanels() const
{
    return
        (isLeftPanelOpened() ? LeftPanel : NoPanel)
        | (isTitleOpened() ? TitlePanel : NoPanel)
        | (isTimelineOpened() ? TimelinePanel : NoPanel)
        | (isNotificationsOpened() ? NotificationsPanel : NoPanel)
        ;
}

void WorkbenchUi::setOpenedPanels(Panels panels, bool animate)
{
    ensureAnimationAllowed(animate);

    setLeftPanelOpened(panels & LeftPanel, animate);
    setTitleOpened(panels & TitlePanel, animate);
    setTimelineOpened(panels & TimelinePanel, animate);
    setNotificationsOpened(panels & NotificationsPanel, animate);
}

void WorkbenchUi::tick(int deltaMSecs)
{
    if (m_timeline->item->zooming() == QnNavigationItem::Zooming::None)
        return;

    if (!mainWindow()->scene())
        return;

    auto slider = m_timeline->item->timeSlider();

    QPointF pos;
    if (slider->windowStart() <= slider->sliderTimePosition() && slider->sliderTimePosition() <= slider->windowEnd())
        pos = slider->positionFromTime(slider->sliderTimePosition(), true);
    else
        pos = slider->rect().center();

    QGraphicsSceneWheelEvent event(QEvent::GraphicsSceneWheel);
    const int zooming = (m_timeline->item->zooming() == QnNavigationItem::Zooming::In) ? 1 : -1;
    // 360 degrees per sec, x8 since delta is measured in in eighths (1/8s) of a degree.
    event.setDelta(360 * 8 * (deltaMSecs * zooming) / 2000);
    event.setPos(pos);
    event.setScenePos(slider->mapToScene(pos));
    mainWindow()->scene()->sendEvent(slider, &event);
}

void WorkbenchUi::at_freespaceAction_triggered()
{
    if (!NX_ASSERT(!qnRuntime->isAcsMode(), "This function must not be called in ActiveX mode."))
        return;

    QAction *fullScreenAction = action(menu::EffectiveMaximizeAction);

    bool isFullscreen = fullScreenAction->isChecked();

    if (!m_inFreespace)
    {
        m_inFreespace = isFullscreen
           && (isLeftPanelPinned() && !isLeftPanelOpened())
           && !isTitleOpened()
           && (isNotificationsPinned() && !isNotificationsOpened())
           && (isTimelinePinned() && !isTimelineOpened());
    }

    if (!m_inFreespace)
    {
        storeSettings();

        if (!isFullscreen)
            fullScreenAction->setChecked(true);

        setLeftPanelOpened(false, isFullscreen);
        setTitleOpened(false, isFullscreen);
        setTimelineOpened(false, isFullscreen);
        setNotificationsOpened(false, isFullscreen);

        m_inFreespace = true;
    }
    else
    {
        loadSettings(/*animated*/isFullscreen, false);
        m_inFreespace = false;
    }
}

void WorkbenchUi::at_fullscreenResourceAction_triggered()
{
    if (!NX_ASSERT(!qnRuntime->isAcsMode(), "This function must not be called in ActiveX mode."))
        return;

    QAction *fullScreenAction = action(menu::EffectiveMaximizeAction);

    bool isFullscreen = fullScreenAction->isChecked();

    if (!m_inFreespace)
    {
        m_inFreespace = isFullscreen
           && (isLeftPanelPinned() && !isLeftPanelOpened())
           && !isTitleOpened()
           && (isNotificationsPinned() && !isNotificationsOpened())
           && (isTimelinePinned() && !isTimelineOpened());
    }

    if (!m_inFreespace)
    {
        storeSettings();

        if (!isFullscreen)
            fullScreenAction->setChecked(true);

        if (QnWorkbenchItem* item = workbench()->item(Qn::CentralRole))
            workbench()->setItem(Qn::ZoomedRole, item);

        setLeftPanelOpened(false, isFullscreen);
        setTitleOpened(false, isFullscreen);
        setTimelineOpened(false, isFullscreen);
        setNotificationsOpened(false, isFullscreen);

        m_inFreespace = true;
    }
    else
    {
        if (workbench()->item(Qn::ZoomedRole))
            workbench()->setItem(Qn::ZoomedRole, nullptr);

        loadSettings(/*animated*/isFullscreen, false);
        m_inFreespace = false;
    }
}

void WorkbenchUi::loadSettings(bool animated, bool useDefault /*unused?*/)
{
    if (useDefault)
        m_settings = Qn::defaultPaneSettings();

    setLeftPanelOpened(m_settings[Qn::WorkbenchPane::Tree].state == Qn::PaneState::Opened, animated);
    if (m_leftPanel && !ini().newPanelsLayout)
        m_leftPanel->setPanelSize(m_settings[Qn::WorkbenchPane::Tree].span);
    setTitleOpened(m_settings[Qn::WorkbenchPane::Title].state == Qn::PaneState::Opened, animated);
    if (ini().enableMultiSystemTabBar)
    {
        const bool expanded = m_settings[Qn::WorkbenchPane::Title].expanded;
        m_title->setExpanded(expanded);
        if (expanded)
            mainWindow()->titleBar()->expand();
        else
            mainWindow()->titleBar()->collapse();
    }
    setTimelineOpened(m_settings[Qn::WorkbenchPane::Navigation].state == Qn::PaneState::Opened, animated);
    setNotificationsOpened(m_settings[Qn::WorkbenchPane::Notifications].state == Qn::PaneState::Opened, animated);
    if (m_notifications && !ini().newPanelsLayout)
        m_notifications->setPanelSize(m_settings[Qn::WorkbenchPane::Notifications].span);
    setCalendarOpened(m_settings[Qn::WorkbenchPane::Calendar].state == Qn::PaneState::Opened, animated);
    if (m_timeline)
    {
        m_timeline->setThumbnailsState(
            m_settings[Qn::WorkbenchPane::Thumbnails].state == Qn::PaneState::Opened,
            m_settings[Qn::WorkbenchPane::Thumbnails].span);
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
        if (widget->isInfoVisible()) //< TODO: #sivanov Wrong place?
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

            const auto panels = m_timeline->isPinnedManually()
                || newWidget->options().testFlag(QnResourceWidget::DisplayMotion)
                || newWidget->options().testFlag(QnResourceWidget::DisplayAnalyticsObjects)
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
        m_title->setGeometry(QRect(
            0,
            m_title->geometry().y(),
            rect.width(),
            m_title->sizeHint().height()));
    }

    if (m_notifications)
    {
        m_notifications->stopAnimations();
        m_notifications->setX(rect.right() + (isNotificationsOpened() ? -m_notifications->geometry().width() : 1.0 /* Just in case. */));
    }

    if (m_leftPanel)
        m_leftPanel->stopAnimations();

    if (m_calendar)
        m_calendar->stopAnimations();

    updateLeftPanelGeometry();
    updateNotificationsGeometry();
    updateLayoutPanelGeometry();
    updateFpsGeometry();
    updateViewportMargins(false);
}

void WorkbenchUi::createControlsWidget()
{
    m_controlsWidget = new QnGuiElementsWidget();
    m_controlsWidget->setObjectName("UIControlsLayer");
    m_controlsWidget->setAcceptedMouseButtons(Qt::NoButton);
    mainWindow()->scene()->addItem(m_controlsWidget);
    display()->setLayer(m_controlsWidget, QnWorkbenchDisplay::UiLayer);

    installEventHandler(m_controlsWidget, QEvent::WindowDeactivate, this,
        [this]() { mainWindow()->scene()->setActiveWindow(m_controlsWidget); });

    m_connections << connect(m_controlsWidget, &QGraphicsWidget::geometryChanged, this, &WorkbenchUi::at_controlsWidget_geometryChanged);
}

#pragma endregion Main controls widget methods

#pragma region TreeWidget

void WorkbenchUi::setLeftPanelVisible(bool visible, bool animate)
{
    if (m_leftPanel)
        m_leftPanel->setVisible(visible, animate);
}

bool WorkbenchUi::isLeftPanelPinned() const
{
    return m_leftPanel && m_leftPanel->isPinned();
}

bool WorkbenchUi::isLeftPanelVisible() const
{
    return m_leftPanel && m_leftPanel->isVisible();
}

bool WorkbenchUi::isLeftPanelOpened() const
{
    return m_leftPanel && m_leftPanel->isOpened();
}

void WorkbenchUi::setLeftPanelOpened(bool opened, bool animate)
{
    if (m_leftPanel)
        m_leftPanel->setOpened(opened, animate);
}

QRectF WorkbenchUi::updatedLeftPanelGeometry(
    const QRectF& treeGeometry, const QRectF& titleGeometry, const QRectF& sliderGeometry)
{
    QPointF pos(treeGeometry.x(), qMax(titleGeometry.bottom(), 0.0));
    qreal bottom = isTimelineVisible()
        ? qMin(sliderGeometry.y(), m_controlsWidgetRect.bottom())
        : m_controlsWidgetRect.bottom();

    QSizeF size(treeGeometry.width(), bottom - pos.y());

    return QRectF(pos, size);
}

void WorkbenchUi::updateLeftPanelGeometry()
{
    if (!m_leftPanel)
        return;

    updateLeftPanelMaximumWidth();

    QRectF titleGeometry = (m_title && m_title->isVisible())
        ? m_title->geometry()
        : QRectF();

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedLeftPanelGeometry(m_leftPanel->geometry(), titleGeometry,
        m_timeline->item->geometry());

    const auto setYAndHeight =
        [this](qreal y, qreal height)
        {
            if (auto tree = qobject_cast<ResourceTreeWorkbenchPanel*>(m_leftPanel))
                tree->setYAndHeight(y, height);
            else if (auto lp = qobject_cast<LeftWorkbenchPanel*>(m_leftPanel))
                lp->setYAndHeight(y, height);
            else
                NX_ASSERT(false);
        };

    setYAndHeight(geometry.topLeft().y(), static_cast<int>(geometry.height()));

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline->isVisible() && isLeftPanelVisible())
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
    geometry = updatedLeftPanelGeometry(m_leftPanel->geometry(), titleEffectiveGeometry,
        QRectF(sliderPos, m_timeline->item->size()));

    if (qFuzzyEquals(geometry, m_leftPanel->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_leftPanel->geometry().height())
        return;

    setYAndHeight(geometry.topLeft().y(), static_cast<int>(geometry.height()));
}

void WorkbenchUi::updateLeftPanelMaximumWidth()
{
    const auto leftPanel = qobject_cast<LeftWorkbenchPanel*>(m_leftPanel);
    if (!leftPanel)
        return;

    // Avoid shrinking the left panel when it is invisible. Do not use isWorkbenchVisible()
    // here because it gives wrong result (visible == true) when the application starts.
    if (!mainWindow()->view()->isVisible())
        return;

    const auto rightLimit = m_notifications && m_notifications->isVisible()
        ? m_notifications->geometry().x()
        : mainWindow()->view()->width();

    leftPanel->setMaximumAllowedWidth(rightLimit - kReservedSceneWidth);
}

void WorkbenchUi::createLeftPanelWidget(const QnPaneSettings& settings)
{
    if (ini().newPanelsLayout)
    {
        m_leftPanel = new LeftWorkbenchPanel(settings, mainWindow()->view(), m_controlsWidget, this);
    }
    else
    {
        m_leftPanel = new ResourceTreeWorkbenchPanel(settings, mainWindow()->view(), m_controlsWidget,
            this);
    }

    m_connections << connect(m_leftPanel, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened && m_leftPanel->isPinned())
                m_inFreespace = false;
        });

    m_connections << connect(m_leftPanel, &AbstractWorkbenchPanel::visibleChanged, this,
        &WorkbenchUi::updateLeftPanelGeometry);

    m_connections << connect(m_leftPanel, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    m_connections << connect(m_leftPanel, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    m_connections << connect(m_leftPanel, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateViewportMarginsAnimated);
    m_connections << connect(m_leftPanel, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateLayoutPanelGeometry);

    installEventHandler(mainWindow()->view(), {QEvent::Show, QEvent::Resize}, this,
        &WorkbenchUi::updateLeftPanelMaximumWidth);
}

#pragma endregion Tree widget methods

#pragma region TitleWidget

bool WorkbenchUi::isTitleVisible() const
{
    return m_title && m_title->isVisible();
}

void WorkbenchUi::setTitleUsed(bool used)
{
    m_titleIsUsed = used;

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
    m_connections << connect(m_title, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened)
                m_inFreespace = false;
        });

    m_connections << connect(m_title, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    m_connections << connect(m_title, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    m_connections << connect(m_title, &AbstractWorkbenchPanel::visibleChanged, this,
        [this](bool /*value*/, bool animated)
        {
            updateLeftPanelGeometry();
            updateNotificationsGeometry();
            updateLayoutPanelGeometry();
            updateFpsGeometry();
            updateViewportMargins(animated);
        });

    m_connections << connect(m_title, &AbstractWorkbenchPanel::geometryChanged, this,
        [this]
        {
            updateLeftPanelGeometry();
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

    m_connections << connect(m_layoutPanel, &AbstractWorkbenchPanel::geometryChanged,
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
        ? m_title->effectiveGeometry()
        : QRectF();

    const auto notificationsLeft = m_notifications && m_notifications->isVisible()
        ? m_notifications->effectiveGeometry().left()
        : m_controlsWidgetRect.right();

    const auto leftRight = m_leftPanel && m_leftPanel->isVisible()
        ? m_leftPanel->effectiveGeometry().right()
        : m_controlsWidgetRect.left();

    const auto topLeft = QPointF(leftRight, titleGeometry.bottom());
    const auto size = QSizeF(notificationsLeft - leftRight, 0); // TODO #ynikitenkov: add height handling
    m_layoutPanel->widget()->setGeometry(QRectF(topLeft, size));
}

void WorkbenchUi::updateNotificationsGeometry()
{
    if (!m_notifications)
        return;

    updateLeftPanelMaximumWidth();

    /* Update painting rect the "fair" way. */
    QRectF titleGeometry = (m_title && m_title->isVisible())
        ? m_title->geometry()
        : QRectF();

    QRectF geometry = updatedNotificationsGeometry(
        m_notifications->geometry(),
        titleGeometry);

    /* Always change position. */
    m_notifications->setPos(geometry.topLeft().toPoint());

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
    geometry = updatedNotificationsGeometry(m_notifications->geometry(),
        titleEffectiveGeometry);

    if (qFuzzyEquals(geometry, m_notifications->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_notifications->geometry().height())
        return;

    m_notifications->resize(geometry.size().toSize());
}

void WorkbenchUi::createNotificationsWidget(const QnPaneSettings& settings)
{
    m_notifications = new NotificationsWorkbenchPanel(settings, mainWindow()->view(), m_controlsWidget, this);
    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened && m_notifications->isPinned())
                m_inFreespace = false;
        });
    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::visibleChanged, this,
        &WorkbenchUi::updateNotificationsGeometry);
    m_connections << connect(m_notifications, &NotificationsWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateNotificationsGeometry);

    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateViewportMarginsAnimated);
    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateFpsGeometry);
    m_connections << connect(m_notifications, &AbstractWorkbenchPanel::geometryChanged, this,
        &WorkbenchUi::updateLayoutPanelGeometry);
}

#pragma endregion Notifications widget methods

#pragma region CalendarWidget

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

    // There is a signal that updates visibility if the calendar receives new data.
    bool calendarEmpty = navigator()->calendar()->isEmpty();
    // TODO: #sivanov Refactor to the same logic as timeline.

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
    const QPoint kCalendarMargins{8, 12};

    QRectF geometry = m_calendar->geometry();
    geometry.moveRight(m_controlsWidgetRect.right());
    geometry.moveBottom(m_timeline->effectiveGeometry().top());
    geometry.translate(-kCalendarMargins);
    m_calendar->setPosition(geometry.topLeft().toPoint());
}

void WorkbenchUi::createCalendarWidget(const QnPaneSettings& settings)
{
    m_calendar = new CalendarWorkbenchPanel(settings, mainWindow()->view(), m_controlsWidget, this);

    // TODO: #sivanov Refactor indirect dependency.
    m_connections << connect(navigator()->calendar(), &TimelineCalendarWidget::emptyChanged, this,
        &WorkbenchUi::updateCalendarVisibilityAnimated);

    m_connections << connect(m_calendar, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    m_connections << connect(m_calendar, &AbstractWorkbenchPanel::hoverLeft, this,
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

    m_connections << connect(m_timeline, &AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened, bool animated)
        {
            if (opened && m_timeline->isPinned())
                m_inFreespace = false;
            updateCalendarVisibility(animated);
        });

    m_connections << connect(m_timeline, &AbstractWorkbenchPanel::visibleChanged, this,
        [this](bool /*value*/, bool animated)
        {
            updateLeftPanelGeometry();
            updateNotificationsGeometry();
            updateCalendarVisibility(animated);
            updateViewportMargins(animated);
        });

    m_connections << connect(m_timeline, &AbstractWorkbenchPanel::hoverEntered, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);
    m_connections << connect(m_timeline, &AbstractWorkbenchPanel::hoverLeft, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    m_connections << connect(m_timeline, &AbstractWorkbenchPanel::geometryChanged, this,
        [this]
        {
            updateLeftPanelGeometry();
            updateNotificationsGeometry();
            updateViewportMargins();
            updateCalendarGeometry();
        });

    m_connections << connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &WorkbenchUi::updateControlsVisibilityAnimated);

    m_connections << connect(action(menu::ToggleShowreelModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            /// If tour mode is going to be turned on, focus should be forced to main window
            /// because otherwise we can't cancel tour mode by clicking any key (in some cases)
            if (toggled)
                mainWindowWidget()->setFocus();
        });

    m_connections << connect(action(menu::ToggleShowreelModeAction), &QAction::toggled, this,
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
        m_connections << connect(m_title, &AbstractWorkbenchPanel::geometryChanged, this, updateDebugGeometry);
        debugLabel = m_debugOverlayLabel;
        previousMsgHandler = qInstallMessageHandler(uiMsgHandler);
    }
    updateDebugGeometry();

    m_connections << connect(action(menu::ShowDebugOverlayAction), &QAction::toggled, this,
        [&](bool toggled) { m_debugOverlayLabel->setVisible(toggled); });
}

#pragma endregion Debug overlay methods

#pragma region FpsWidget

bool WorkbenchUi::isPerformanceInfoVisible() const
{
    return m_performanceInfoWidget->isVisible();
}

void WorkbenchUi::setPerformanceInfoVisible(bool performanceInfoVisible)
{
    if (performanceInfoVisible == isPerformanceInfoVisible())
        return;

    m_performanceInfoWidget->setVisible(performanceInfoVisible);
    m_performanceInfoWidget->setText({});

    action(menu::ShowFpsAction)->setChecked(performanceInfoVisible);
    appContext()->performanceMonitor()->setVisible(performanceInfoVisible);
}

void WorkbenchUi::setDebugInfoVisible(bool debugInfoVisible)
{
    appContext()->performanceMonitor()->setDebugInfoVisible(debugInfoVisible);
}

void WorkbenchUi::updateFpsGeometry()
{
    qreal right = m_notifications && m_notifications->isVisible()
        ? m_notifications->geometry().left()
        : m_controlsWidgetRect.right();

    QRectF titleEffectiveGeometry = (m_title && m_title->isVisible())
        ? m_title->effectiveGeometry()
        : QRectF();

    QPointF pos = QPointF(
        right - m_performanceInfoWidget->size().width(),
        titleEffectiveGeometry.bottom());

    if (qFuzzyEquals(pos, m_performanceInfoWidget->pos()))
        return;

    m_performanceInfoWidget->setPos(pos);
}

void WorkbenchUi::createFpsWidget()
{
    m_performanceInfoWidget = new PerformanceInfoWidget{m_controlsWidget, {}, windowContext()};
    m_performanceInfoWidget->setText("....");
    m_performanceInfoWidget->setVisible(false); // Visibility is controlled via setFpsVisible() method.

    updateFpsGeometry();
    setPaletteColor(m_performanceInfoWidget, QPalette::Window, Qt::transparent);
    setPaletteColor(m_performanceInfoWidget, QPalette::WindowText, QColor(63, 159, 216));
    display()->setLayer(m_performanceInfoWidget, QnWorkbenchDisplay::MessageBoxLayer);

    m_connections << connect(action(menu::ShowFpsAction), &QAction::toggled, this, &WorkbenchUi::setPerformanceInfoVisible);
    m_connections << connect(m_performanceInfoWidget, &QGraphicsWidget::geometryChanged, this, &WorkbenchUi::updateFpsGeometry);

    setDebugInfoVisible(ini().developerMode);
}

#pragma endregion Fps widget methods

} // namespace nx::vms::client::desktop
