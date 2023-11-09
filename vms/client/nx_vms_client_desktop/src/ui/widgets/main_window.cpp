// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window.h"

#include <QtCore/QFile>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtGui/QFileOpenEvent>
#include <QtGui/QWindowStateChangeEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QToolButton>

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/self_updater.h>
#include <core/resource/file_processor.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/audio_dispatcher.h>
#include <nx/vms/client/desktop/debug_utils/menu/debug_actions_handler.h>
#include <nx/vms/client/desktop/export/workbench/workbench_export_handler.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_action_handler.h>
#include <nx/vms/client/desktop/manual_device_addition/workbench/workbench_manual_device_addition_handler.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/radass/radass_action_handler.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/rules/vms_rules_action_handler.h>
#include <nx/vms/client/desktop/session_manager/session_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/screen_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/cloud_actions_handler.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/system_merge/incompatible_servers_action_handler.h>
#include <nx/vms/client/desktop/workbench/handlers/alarm_layout_handler.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_executor.h>
#include <nx/vms/client/desktop/workbench/handlers/workbench_action_executor.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <nx/vms/client/desktop/workbench/workbench_ui.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/system_uri.h>
#include <ui/common/frame_section.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/main_window_title_bar_widget.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/workaround/vsync_workaround.h>
#include <ui/workbench/handlers/resource_tree_settings_action_handler.h>
#include <ui/workbench/handlers/startup_actions_handler.h>
#include <ui/workbench/handlers/workbench_action_handler.h>
#include <ui/workbench/handlers/workbench_bookmarks_handler.h>
#include <ui/workbench/handlers/workbench_ptz_handler.h>
#include <ui/workbench/handlers/workbench_resource_grouping_action_handler.h>
#include <ui/workbench/handlers/workbench_resources_settings_handler.h>
#include <ui/workbench/handlers/workbench_screenshot_handler.h>
#include <ui/workbench/handlers/workbench_text_overlays_handler.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h>
#include <ui/workbench/handlers/workbench_webpage_handler.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/watchers/workbench_bookmark_tags_watcher.h>
#include <ui/workbench/watchers/workbench_item_bookmarks_watcher.h>
#include <ui/workbench/watchers/workbench_ptz_dialog_watcher.h>
#include <ui/workbench/watchers/workbench_resources_changes_watcher.h>
#include <ui/workbench/watchers/workbench_server_port_watcher.h>
#include <ui/workbench/watchers/workbench_user_inactivity_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#ifdef Q_OS_WIN
    #include <nx/vms/client/desktop/platforms/windows/gdi_win.h>
#endif

#ifdef Q_OS_MACX
    #include <ui/workaround/mac_utils.h>
#endif

using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMinimalWindowWidth = 1024;
static constexpr int kMinimalWindowHeight = 768;
static constexpr int kSmallMinimalWindowWidth = 400;
static constexpr int kSmallMinimalWindowHeight = 300;

} // namespace

// These functions are used from mac_utils.mm
#ifdef Q_OS_MACX
extern "C" {

void disable_animations(void* qnmainwindow)
{
    MainWindow* mainwindow = (MainWindow*) qnmainwindow;
    mainwindow->setAnimationsEnabled(false);
}

void enable_animations(void* qnmainwindow)
{
    MainWindow* mainwindow = (MainWindow*) qnmainwindow;
    mainwindow->setAnimationsEnabled(true);
}

}
#endif

using namespace ui;

struct MainWindow::Private
{
    Private(MainWindow* owner):
        q(owner)
    {
        screenManager = std::make_unique<ScreenManager>(appContext()->sharedMemoryManager());
        titleBarStateStore.reset(new MainWindowTitleBarStateStore);
    }

    MainWindow* q;
    std::unique_ptr<ScreenManager> screenManager;
    QSharedPointer<MainWindowTitleBarStateStore> titleBarStateStore;
};

MainWindow::MainWindow(WindowContext* context, QWidget* parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    WindowContextAware(context),
    d(new Private(this))
{
    if (qnRuntime->isDesktopMode())
        m_welcomeScreen = new WelcomeScreen(context, this);

    QnHiDpiWorkarounds::init();

#ifdef Q_OS_MACX
    // Workaround prevents hangs on moving to the fullscreen mode in and out.

    const auto handler = nx::utils::guarded(this,
        [this](bool inProgress)
        {
            if (inProgress)
            {
                setUpdatesEnabled(false);
                return;
            }

            static constexpr int kSomeSmallDelay = 50;
            executeDelayedParented([this]() { setUpdatesEnabled(true); }, kSomeSmallDelay, this);
        });

    setFullscreenTransitionHandler(this, handler);

    // Since we have patch qt563_macos_window_level.patch we have to discard hidesOnDeactivate
    // flag for main window. Otherwise, each time it looses focus it becomes hidden.
    setHidesOnDeactivate(winId(), false);
#endif

    setAttribute(Qt::WA_AlwaysShowToolTips);
    /* And file open events on Mac. */
    installEventHandler(qApp, QEvent::FileOpen, this, &MainWindow::at_fileOpenSignalizer_activated);

    /* Set up properties. */
    setWindowTitle(QString());
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark4"));

    /* Initialize animations manager. */
    workbenchContext()->instance<ui::workbench::Animations>();

    if (!qnRuntime->isVideoWallMode())
    {
        const bool smallWindow = qnRuntime->lightMode().testFlag(Qn::LightModeSmallWindow);
        setMinimumWidth(smallWindow ? kSmallMinimalWindowWidth : kMinimalWindowWidth);
        setMinimumHeight(smallWindow ? kSmallMinimalWindowHeight : kMinimalWindowHeight);
    }

    /* Set up scene & view. */
    m_scene.reset(new QnGraphicsScene(this));

    connect(workbench(),
        &Workbench::currentLayoutAboutToBeChanged,
        this,
        [this]()
        {
            if (auto resource = workbench()->currentLayoutResource())
                resource->disconnect(this);
        });
    connect(workbench(),
        &Workbench::currentLayoutChanged,
        this,
        [this]()
        {
            if (auto resource = workbench()->currentLayoutResource())
            {
                connect(resource.data(),
                    &QnLayoutResource::backgroundImageChanged,
                    this,
                    &MainWindow::updateHelpTopic);
            }
            updateHelpTopic();
        });
    connect(action(menu::ToggleShowreelModeAction), &QAction::toggled, this, &MainWindow::updateHelpTopic);
    updateHelpTopic();

    m_view.reset(new QnGraphicsView(m_scene.data(), this));
    m_view->setAutoFillBackground(true);
    // This attribute is required to combine QGLWidget (main scene) and QQuickWidget (welcome
    // screen) in one window. Without it QGLWidget content may be not displayed in some OSes.
    m_view->setAttribute(Qt::WA_DontCreateNativeAncestors);

    /* Set up model & control machinery. */
    display()->initialize(m_scene.data(), m_view.data());

    // Controls which are initialized further access mainWindow() through the context. Set it here
    // to avoid passing main window pointer to constructors (though it will certainly be more
    // correct).
    workbenchContext()->setMainWindow(this);

    if (qnRuntime->isVideoWallMode())
        display()->setNormalMarginFlags({});
    else
        display()->setNormalMarginFlags(Qn::MarginsAffectSize | Qn::MarginsAffectPosition);

    m_titleBar = new QnMainWindowTitleBarWidget(windowContext(), this);
    m_titleBar->setStateStore(d->titleBarStateStore);
    m_controller.reset(new QnWorkbenchController(context, this));
    if (qnRuntime->isVideoWallMode())
        m_controller->setMenuEnabled(false);
    m_ui.reset(new WorkbenchUi(context, this));
    if (qnRuntime->isVideoWallMode())
        m_ui->setFlags(WorkbenchUi::HideWhenZoomed | WorkbenchUi::HideWhenNormal );
    else
        m_ui->setFlags(WorkbenchUi::HideWhenZoomed | WorkbenchUi::AdjustMargins);

    // Initialize action handlers.
    workbenchContext()->instance<ui::workbench::ActionHandler>();
    workbenchContext()->instance<NotificationActionExecutor>();
    workbenchContext()->instance<WorkbenchActionExecutor>();
    workbenchContext()->instance<QnWorkbenchScreenshotHandler>();
    workbenchContext()->instance<WorkbenchExportHandler>();
    workbenchContext()->instance<QnWorkbenchPtzHandler>();
    workbenchContext()->instance<QnWorkbenchVideoWallHandler>();
    workbenchContext()->instance<QnWorkbenchWebPageHandler>();
    workbenchContext()->instance<IncompatibleServersActionHandler>();
    workbenchContext()->instance<QnWorkbenchResourcesSettingsHandler>();
    workbenchContext()->instance<QnWorkbenchBookmarksHandler>();
    workbenchContext()->instance<WorkbenchManualDeviceAdditionHandler>();
    workbenchContext()->instance<AlarmLayoutHandler>();
    workbenchContext()->instance<QnWorkbenchTextOverlaysHandler>();
    workbenchContext()->instance<CloudActionsHandler>();
    workbenchContext()->instance<LookupListActionHandler>();
    workbenchContext()->instance<RadassActionHandler>();
    workbenchContext()->instance<StartupActionsHandler>();
    workbenchContext()->instance<ResourceGroupingActionHandler>();
    workbenchContext()->instance<ui::workbench::ResourceTreeSettingsActionHandler>();
    workbenchContext()->instance<rules::VmsRulesActionHandler>();

    workbenchContext()->instance<QnWorkbenchPtzDialogWatcher>();

    workbenchContext()->instance<QnWorkbenchResourcesChangesWatcher>();
    workbenchContext()->instance<QnWorkbenchBookmarkTagsWatcher>();
    workbenchContext()->instance<QnWorkbenchItemBookmarksWatcher>();
    workbenchContext()->instance<QnTimelineBookmarksWatcher>();
    workbenchContext()->instance<QnWorkbenchServerPortWatcher>();

    /* Set up watchers. */
    workbenchContext()->instance<QnWorkbenchUserInactivityWatcher>()->setMainWindow(this);

    const auto updateTimeMode =
        []()
        {
            const auto newMode = (appContext()->localSettings()->timeMode() == Qn::ClientTimeMode)
                ? nx::vms::client::core::ServerTimeWatcher::clientTimeMode
                : nx::vms::client::core::ServerTimeWatcher::serverTimeMode;
            for (auto systemContext: appContext()->systemContexts())
                systemContext->serverTimeWatcher()->setTimeMode(newMode);
        };
    connect(&appContext()->localSettings()->timeMode,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        updateTimeMode);

    // Apply already loaded time mode settings
    updateTimeMode();

    /* Set up actions. Only these actions will be available through hotkeys. */
    addAction(action(menu::NotificationsTabAction));
    addAction(action(menu::ResourcesTabAction));
    addAction(action(menu::SearchResourcesAction));
    addAction(action(menu::MotionTabAction));
    addAction(action(menu::SwitchMotionTabAction));
    addAction(action(menu::BookmarksTabAction));
    addAction(action(menu::EventsTabAction));
    addAction(action(menu::ObjectsTabAction));
    addAction(action(menu::ObjectSearchModeAction));
    addAction(action(menu::NextLayoutAction));
    addAction(action(menu::PreviousLayoutAction));
    addAction(action(menu::SaveCurrentLayoutAction));
    addAction(action(menu::SaveCurrentLayoutAsAction));
    addAction(action(menu::SaveCurrentVideoWallReviewAction));
    addAction(action(menu::CloseAllWindowsAction));
    addAction(action(menu::ExitAction));
    addAction(action(menu::EscapeHotkeyAction));
    addAction(action(menu::FullscreenMaximizeHotkeyAction));
    addAction(action(menu::AboutAction));
    addAction(action(menu::PreferencesGeneralTabAction));
    addAction(action(menu::OpenBookmarksSearchAction));
    addAction(action(menu::OpenBusinessLogAction));
    addAction(action(menu::CameraListAction));
    addAction(action(menu::BusinessEventsAction));
    addAction(action(menu::OpenFileAction));
    addAction(action(menu::OpenNewTabAction));
    addAction(action(menu::OpenNewWindowAction));
    addAction(action(menu::OpenWelcomeScreenAction));
    addAction(action(menu::CloseLayoutAction));
    addAction(action(menu::MainMenuAction));
    addAction(action(menu::OpenLoginDialogAction));
    addAction(action(menu::DisconnectMainMenuAction));
    addAction(action(menu::OpenInFolderAction));
    addAction(action(menu::RemoveLayoutItemAction));
    addAction(action(menu::RemoveLayoutItemFromSceneAction));
    addAction(action(menu::RemoveFromServerAction));
    addAction(action(menu::DeleteVideoWallItemAction));
    addAction(action(menu::DeleteVideowallMatrixAction));
    addAction(action(menu::RemoveShowreelAction));
    addAction(action(menu::SelectAllAction));
    addAction(action(menu::CheckFileSignatureAction));
    addAction(action(menu::TakeScreenshotAction));
    addAction(action(menu::AdjustVideoAction));
    addAction(action(menu::ToggleShowreelModeAction));
    addAction(action(menu::DebugIncrementCounterAction));
    addAction(action(menu::DebugDecrementCounterAction));
    addAction(action(menu::DebugControlPanelAction));
    addAction(action(menu::SystemAdministrationAction));
    if (auto screenRecordingAction = action(menu::ToggleScreenRecordingAction))
        addAction(screenRecordingAction);
    addAction(action(menu::ShowFpsAction));
    addAction(action(menu::OpenNewSceneAction));
    addAction(action(menu::OpenVmsRulesDialogAction));
    addAction(action(menu::CreateNewCustomGroupAction));
    addAction(action(menu::RemoveCustomGroupAction));
    addAction(action(menu::PreviousFrameAction));
    addAction(action(menu::NextFrameAction));
    addAction(action(menu::JumpToStartAction));
    addAction(action(menu::JumpToEndAction));
    addAction(action(menu::VolumeUpAction));
    addAction(action(menu::VolumeDownAction));
    addAction(action(menu::ToggleMuteAction));
    addAction(action(menu::JumpToLiveAction));
    addAction(action(menu::ToggleSyncAction));
    addAction(action(menu::ToggleSmartSearchAction));
    addAction(action(menu::ToggleInfoAction));
    addAction(action(menu::FreespaceAction));
    addAction(action(menu::ShowDebugOverlayAction));
    addAction(action(menu::DebugToggleElementHighlight));

    connect(action(menu::MaximizeAction), &QAction::toggled, this,
        &MainWindow::setMaximized);
    connect(action(menu::FullscreenAction), &QAction::toggled, this,
        &MainWindow::setFullScreen);
    connect(action(menu::MinimizeAction), &QAction::triggered, this,
        [this]()
        {
            showMinimized();
            // Widget updates are completely disabled while application in minimized state to
            // avoid unnecessary heavy computations which are result of Qt quirks combined with
            // player and rendering architecture.
            setUpdatesEnabled(!isMinimized());
        });

    connect(action(menu::FullscreenMaximizeHotkeyAction), &QAction::triggered,
        action(menu::EffectiveMaximizeAction), &QAction::trigger);

    menu()->setTargetProvider(m_ui.data());

    /* Layouts. */

    m_viewLayout = new QStackedLayout();
    m_viewLayout->setContentsMargins(0, 0, 0, 0);

    m_globalLayout = new QVBoxLayout(this);
    m_globalLayout->setContentsMargins(0, 0, 0, 0);
    m_globalLayout->setSpacing(0);

    m_globalLayout->addWidget(m_titleBar);
    m_globalLayout->addLayout(m_viewLayout, 1);

    setLayout(m_globalLayout);

    if (m_welcomeScreen)
        m_viewLayout->addWidget(m_welcomeScreen);

    m_viewLayout->addWidget(m_view.data());

    // Post-initialize.
    if (nx::build_info::isMacOsX())
        setOptions(Options());
    else
        setOptions(TitleBarDraggable);

    // Initialize system-wide menu
    if (nx::build_info::isMacOsX())
        menu()->newMenu(menu::MainScope, this);

    if (ini().limitFrameRate && ini().enableVSyncWorkaround)
    {
        /* VSync workaround must always be enabled to limit fps usage in following cases:
         * * VSync is not supported by drivers
         * * VSync is disabled in drivers
         * * double buffering is disabled in drivers or in our program
         */
        auto vsyncWorkaround = new QnVSyncWorkaround(m_view->viewport(), this);
        Q_UNUSED(vsyncWorkaround);
    }

    installEventHandler(m_view->viewport(), QEvent::Show, this,
        [this]
        {
            if (m_initialized)
                return;

            const auto audioDispatcher = AudioDispatcher::instance();
            const auto window = windowHandle();
            audioDispatcher->requestAudio(window);
            audioDispatcher->setPrimaryAudioSource(window);

            m_initialized = true;
            setWelcomeScreenVisible(true);
        });

    updateWidgetsVisibility();
}

MainWindow::~MainWindow()
{
    // Deinitialize display before scene is destroyed.
    display()->deinitialize();
}

QGraphicsScene* MainWindow::scene() const
{
    return m_scene.data();
}

QGraphicsView* MainWindow::view() const
{
    return m_view.data();
}

QWidget *MainWindow::viewport() const {
    return m_view->viewport();
}

ScreenManager* MainWindow::screenManager() const
{
    return d->screenManager.get();
}

WelcomeScreen* MainWindow::welcomeScreen() const
{
    return m_welcomeScreen;
}

bool MainWindow::isTitleVisible() const
{
    if (!qnRuntime->isDesktopMode())
        return false;

    return m_titleVisible || m_welcomeScreenVisible;
}

void MainWindow::updateWidgetsVisibility()
{
    m_titleBar->setTabBarStuffVisible(!m_welcomeScreenVisible);

    const auto switchWidgetsCallback =
        [this]
        {
            const auto currentWidget = m_welcomeScreen && m_welcomeScreenVisible
                ? static_cast<QWidget*>(m_welcomeScreen)
                : static_cast<QWidget*>(m_view.data());

            if (currentWidget != m_viewLayout->currentWidget())
                m_viewLayout->setCurrentWidget(currentWidget);

            // We cannot synchronize it with QmlResourceBrowserWidget visibility inside it,
            // because it can be isVisible being non-current in QStackedLayout.
            action(menu::SearchResourcesAction)->setEnabled(currentWidget != m_welcomeScreen);
        };

    switchWidgetsCallback();

    // Always show title bar for welcome screen (it does not matter if it is fullscreen).
    m_titleBar->setVisible(isTitleVisible());

    // Fix scene activation state (Qt bug workaround).
    if (m_welcomeScreen && !m_welcomeScreenVisible)
    {
        // QGraphicsScene contains activation counter. On `WindowActivate` counter is increased,
        // on `WindowsDeactivate` (focus change, hide, etc) - decreased. There is scenario when
        // `WindowDeactivate` is called twice (change focus to 'Reconnecting' dialog, then display
        // Welcome Screen. In this case counter goes below zero, and the scene goes permanently
        // inactive.
        // Problem-causing piece of code:
        // ```
        // if (!--d->activationRefCount) { ... }
        // ```
        QEvent e(QEvent::WindowActivate);
        QObject* sceneObject = m_scene.data();
        sceneObject->event(&e);
    }

    updateContentsMargins();
}

void MainWindow::setTitleVisible(bool visible)
{
    if(m_titleVisible == visible)
        return;

    m_titleVisible = visible;

    updateWidgetsVisibility();
}

void MainWindow::setWelcomeScreenVisible(bool visible)
{
    if (!m_welcomeScreen)
        visible = false;

    if (m_welcomeScreenVisible == visible)
        return;

    m_welcomeScreenVisible = visible;
    if (visible && isSystemTabBarUpdating())
    {
        welcomeScreen()->setGlobalPreloaderVisible(true);
    }
    else
    {
        if (ini().enableMultiSystemTabBar)
            m_titleBar->setHomeTabActive(visible);
    }

    updateWidgetsVisibility();
}

bool MainWindow::isWorkbenchVisible() const
{
    NX_ASSERT(m_viewLayout->count() > 0 && m_viewLayout->count() <= 2);
    return !m_welcomeScreenVisible;
}

bool MainWindow::isWelcomeScreenVisible() const
{
    return m_welcomeScreenVisible;
}

void MainWindow::setMaximized(bool maximized)
{
    if(maximized == isMaximized())
        return;

    if(maximized)
        showMaximized();
    else if(isMaximized())
        showNormal();
}

bool MainWindow::isFullScreenMode() const
{
#if defined(Q_OS_MAC)
    return mac_isFullscreen(reinterpret_cast<void*>(winId()));
#else
    return isFullScreen();
#endif
}

void MainWindow::setFullScreen(bool fullScreen)
{
    if(fullScreen == isFullScreenMode())
        return;

    // TODO: #spanasenko Remove it?
    // It was used to guard m_storedGeometry field. Perhaps now it's useless.
    if (m_inFullscreenTransition)
        return;

    QScopedValueRollback<bool> guard(m_inFullscreenTransition, true);

    if (fullScreen)
    {
        showFullScreen();
    }
    else if(isFullScreenMode())
    {
        showNormal();
    }
}

void MainWindow::setAnimationsEnabled(bool enabled) {
    InstrumentManager *manager = InstrumentManager::instance(m_scene.data());
    manager->setAnimationsEnabled(enabled);
}

void MainWindow::showFullScreen() {
#if defined Q_OS_MACX
    mac_showFullScreen((void*)winId(), true);
    updateDecorationsState();

    // We have to disable minimize button in Mac OS for application in fullscreen mode
    action(menu::MinimizeAction)->setEnabled(false);
#else
    QnEmulatedFrameWidget::showFullScreen();
#endif
}

void MainWindow::showNormal() {
#if defined Q_OS_MACX
    mac_showFullScreen((void*)winId(), false);
    updateDecorationsState();
    // We have to enable minimize button in Mac OS for application in non-fullscreen mode only
    action(menu::MinimizeAction)->setEnabled(true);
#else
    QnEmulatedFrameWidget::showNormal();
#endif
}

void MainWindow::updateScreenInfo()
{
    d->screenManager->updateCurrentScreens(this);
}

std::pair<int, bool> MainWindow::calculateHelpTopic() const
{
    if (action(menu::ToggleShowreelModeAction)->isChecked())
        return {HelpTopic::Id::MainWindow_Scene_TourInProgress, true};

    if (auto layout = workbench()->currentLayout())
    {
        if (!layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
            return {HelpTopic::Id::Videowall_Display, false};

        if (layout->isPreviewSearchLayout())
            return {HelpTopic::Id::MainWindow_Scene_PreviewSearch, true};

        if (layout->isShowreelReviewLayout())
            return {HelpTopic::Id::Showreel, true};

        if (QnLayoutResourcePtr resource = layout->resource())
        {
            if (resource->isFile())
                return{HelpTopic::Id::MainWindow_Tree_MultiVideo, true};

            if (resource->hasBackground())
                return {HelpTopic::Id::MainWindow_Scene_EMapping, false};
        }
    }
    return {HelpTopic::Id::MainWindow_Scene, false};
}

void MainWindow::updateHelpTopic()
{
    auto topic = calculateHelpTopic();
    setHelpTopic(m_scene.data(), topic.first, topic.second);
}

void MainWindow::minimize() {
    showMinimized();
}

bool MainWindow::handleOpenFile(const QString &message)
{
    const auto files = message.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const auto resources = QnFileProcessor::createResourcesForFiles(
        QnFileProcessor::findAcceptedFiles(files),
        system()->resourcePool());

    if (resources.isEmpty())
        return false;

    menu()->trigger(menu::DropResourcesAction, resources);
    return true;
}

MainWindow::Options MainWindow::options() const {
    return m_options;
}

void MainWindow::setOptions(Options options) {
    if(m_options == options)
        return;

    m_options = options;
}

void MainWindow::updateDecorationsState()
{
#ifdef Q_OS_MACX
    bool fullScreen = mac_isFullscreen((void*)winId());
#else
    bool fullScreen = isFullScreen();
#endif
    bool maximized = isMaximized();

    action(menu::FullscreenAction)->setChecked(fullScreen);
    action(menu::MaximizeAction)->setChecked(maximized);

#ifdef Q_OS_MACX
    bool uiTitleUsed = fullScreen;
#else
    bool uiTitleUsed = fullScreen || maximized;
#endif

    bool windowTitleUsed = !uiTitleUsed && !qnRuntime->isVideoWallMode() && !qnRuntime->isAcsMode();
    setTitleVisible(windowTitleUsed);
    m_ui->setTitleUsed(uiTitleUsed && !qnRuntime->isVideoWallMode() && !qnRuntime->isAcsMode());
    m_view->setLineWidth(windowTitleUsed ? 0 : 1);
    updateContentsMargins();
}

bool MainWindow::handleKeyPress(int key)
{
    // Qt shortcuts handling works incorrect. If we have a shortcut set for an action, it will
    // block key event passing in any case (even if we did not handle the event).
    if (key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Shift)
        return false;

    const bool isTourRunning = action(menu::ToggleShowreelModeAction)->isChecked();

    if (!isTourRunning)
    {
        if (key == Qt::Key_Space)
        {
            menu()->triggerIfPossible(menu::PlayPauseAction,
                navigator()->currentParameters(menu::TimelineScope));
            return true;
        }

        // Only running tours are handled further.
        return false;
    }

    switch (key)
    {
        case Qt::Key_Backspace:
        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
            menu()->trigger(menu::PreviousLayoutAction);
            break;

        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
            menu()->trigger(menu::NextLayoutAction);
            break;

        case Qt::Key_Escape:
            // Stop Showreel if it is running.
            menu()->trigger(menu::ToggleShowreelModeAction);
            break;
    }
    return true;
}

QnMainWindowTitleBarWidget* MainWindow::titleBar() const
{
    return m_titleBar;
}

bool MainWindow::isSystemTabBarUpdating()
{
    return m_titleBar->isSystemTabBarUpdating();
}

void MainWindow::updateContentsMargins()
{
    m_drawCustomFrame = false;
    m_frameMargins = QMargins{};

    if (nx::build_info::isLinux() && !isFullScreenMode() && !isMaximized())
    {
        // In Linux window managers cannot disable titlebar leaving window border in place.
        // Thus we have to disable decorations completely and draw our own border.
        m_drawCustomFrame = true;
        m_frameMargins = QMargins(2, 2, 2, 2);
    }

    m_globalLayout->setContentsMargins(m_frameMargins);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool MainWindow::event(QEvent* event)
{
    const bool result = base_type::event(event);
    if (event->type() == QnEvent::WinSystemMenu)
        menu()->trigger(menu::MainMenuAction);

    return result;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    menu()->trigger(menu::ExitAction);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        setUpdatesEnabled(!isMinimized());

#if defined(Q_OS_WIN) // Ensure our custom WM_NCCALCSIZE works correctly.
        const auto wscEvent = static_cast<QWindowStateChangeEvent*>(event);
        if (wscEvent->oldState().testFlag(Qt::WindowMinimized) && m_inFullscreen)
            base_type::showFullScreen();
#endif
        m_inFullscreen = windowState().testFlag(Qt::WindowFullScreen);
        updateDecorationsState();
    }

    base_type::changeEvent(event);
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    base_type::paintEvent(event);

    if (m_drawCustomFrame)
    {
        QPainter painter(this);

        painter.setPen(QPen(core::colorTheme()->color("mainWindow.customFrame"), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    base_type::keyPressEvent(event);
    handleKeyPress(event->key());
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);
    updateScreenInfo();
}

void MainWindow::moveEvent(QMoveEvent *event) {
    base_type::moveEvent(event);
    updateScreenInfo();
}

Qt::WindowFrameSection MainWindow::windowFrameSectionAt(const QPoint &pos) const
{
    if (isFullScreenMode() && !isTitleVisible())
        return Qt::NoSection;

    Qt::WindowFrameSection result = Qn::toNaturalQtFrameSection(
            Qn::calculateRectangularFrameSections(
                    rect(),
                    Geometry::eroded(rect(), m_frameMargins),
                    QRect(pos, pos)));

    if (m_options.testFlag(TitleBarDraggable) &&
        result == Qt::NoSection &&
        pos.y() <= m_titleBar->mapTo(this, m_titleBar->rect().bottomRight()).y())
    {
        result = Qt::TitleBarArea;
    }

    return result;
}

void MainWindow::at_fileOpenSignalizer_activated(QObject*, QEvent* event)
{
    if(event->type() != QEvent::FileOpen)
    {
        NX_ASSERT(false, "Expected event of type %1, received an event of type %2.",
            static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    const auto fileEvent = static_cast<QFileOpenEvent *>(event);
    const auto url = fileEvent->url();
    if (!url.isEmpty() && !url.isLocalFile())
        SelfUpdater::runNewClient({url.toString()});
    else
        handleOpenFile(fileEvent->file());
}

#ifdef Q_OS_WIN

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    const auto msg = static_cast<MSG*>(message);
    const auto isMinimized =
        [wnd = msg->hwnd]()
        {
            WINDOWPLACEMENT p{};
            p.length = sizeof(p);
            GetWindowPlacement(wnd, &p);
            return p.showCmd == SW_MINIMIZE
                || p.showCmd == SW_SHOWMINIMIZED
                || p.showCmd == SW_SHOWMINNOACTIVE;
        };

    switch (msg->message)
    {
        // Under Windows, fullscreen OpenGL widgets on the primary screen cause Windows DWM to
        // turn desktop composition off and enter fullscreen mode. To avoid this behavior,
        // an artificial delta is added to make the client area differ from the screen dimensions.
        case WM_NCCALCSIZE:
        {
            if (!isFullScreenMode() || isMinimized())
                return false;

            auto rect = LPRECT(msg->lParam);
            ++rect->right;

            *result = WVR_REDRAW;
            return true;
        }

        // Paint background to avoid white window background flashing when switched from the scene
        // to the welcome screen and back.
        case WM_ERASEBKGND:
        {
            const auto context = HDC(msg->wParam);
            gdi::SolidBrush brush(palette().color(QPalette::Window));

            RECT rect;
            GetClientRect(msg->hwnd, &rect);
            FillRect(context, &rect, brush);

            *result = TRUE;
            return true;
        }

        // Block default activation processing in fullscreen.
        case WM_NCACTIVATE:
        {
            if (!isFullScreenMode() || qnRuntime->isVideoWallMode())
                return false;

            *result = TRUE;
            return true;
        }

        // Filling non-client area in fullscreen with the background color also improves visual
        // behavior.
        case WM_NCPAINT:
        {
            if (!isFullScreenMode() && !isMinimized())
                return false;

            gdi::WindowDC context(msg->hwnd);
            gdi::SolidBrush brush(palette().color(QPalette::Window));

            RECT rect;
            GetWindowRect(msg->hwnd, &rect);

            if (GetObjectType(HGDIOBJ(msg->wParam)) == OBJ_REGION)
            {
                OffsetRgn(HRGN(msg->wParam), -rect.left, -rect.top);
                SelectClipRgn(context, HRGN(msg->wParam));
            }

            OffsetRect(&rect, -rect.left, -rect.top);
            FillRect(context, &rect, brush);

            *result = 0;
            return true;
        }

        default:
            return false;
    }
}

#endif // Q_OS_WIN

} // namespace nx::vms::client::desktop
