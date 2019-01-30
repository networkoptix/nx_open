#include "main_window.h"

#ifdef Q_OS_MACX
#include <ui/workaround/mac_utils.h>
#endif

#include <QtCore/QFile>

#include <QtGui/QFileOpenEvent>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QStackedLayout>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/system_uri.h>

#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/videowall_resource.h>

#include <api/session_manager.h>

#include <ui/common/palette.h>
#include <ui/common/frame_section.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <nx/vms/client/desktop/workbench/handlers/layout_tours_handler.h>
#include <nx/vms/client/desktop/workbench/extensions/workbench_progress_manager.h>
#include <nx/client/core/watchers/server_time_watcher.h>

#include <ui/workbench/workbench_welcome_screen.h>
#include <ui/workbench/handlers/workbench_action_handler.h>
#include <ui/workbench/handlers/workbench_bookmarks_handler.h>
#include <ui/workbench/handlers/workbench_connect_handler.h>
#include <ui/workbench/handlers/workbench_layouts_handler.h>
#include <ui/workbench/handlers/workbench_permissions_handler.h>
#include <ui/workbench/handlers/workbench_screenshot_handler.h>
#include <nx/vms/client/desktop/export/workbench/workbench_export_handler.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/handlers/workbench_ptz_handler.h>
#include <ui/workbench/handlers/workbench_debug_handler.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h>
#include <ui/workbench/handlers/workbench_incompatible_servers_action_handler.h>
#include <ui/workbench/handlers/workbench_resources_settings_handler.h>
#include <ui/workbench/handlers/workbench_alarm_layout_handler.h>
#include <ui/workbench/handlers/workbench_cloud_handler.h>
#include <ui/workbench/handlers/workbench_webpage_handler.h>
#include <ui/workbench/handlers/workbench_screen_recording_handler.h>
#include <ui/workbench/handlers/workbench_text_overlays_handler.h>
#include <nx/vms/client/desktop/radass/radass_action_handler.h>
#include <ui/workbench/handlers/workbench_wearable_handler.h>
#include <ui/workbench/handlers/startup_actions_handler.h>
#include <nx/vms/client/desktop/analytics/analytics_menu_action_handler.h>
#include <nx/vms/client/desktop/manual_device_addition/workbench/workbench_manual_device_addition_handler.h>

#include <ui/workbench/watchers/workbench_user_inactivity_watcher.h>
#include <ui/workbench/watchers/workbench_layout_aspect_ratio_watcher.h>
#include <ui/workbench/watchers/workbench_ptz_dialog_watcher.h>
#include <ui/workbench/watchers/workbench_server_port_watcher.h>
#include <ui/workbench/watchers/workbench_resources_changes_watcher.h>
#include <ui/workbench/watchers/workbench_server_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_bookmark_tags_watcher.h>
#include <ui/workbench/watchers/workbench_bookmarks_watcher.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/watchers/workbench_item_bookmarks_watcher.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_state_manager.h>
#include <nx/vms/client/desktop/workbench/workbench_ui.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_welcome_screen.h>
#include <ui/workaround/hidpi_workarounds.h>

#include <ui/widgets/main_window_title_bar_widget.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/workaround/vsync_workaround.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_message_processor.h>
#include <client/self_updater.h>

#include <client_core/client_core_module.h>

#include <utils/screen_manager.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/utils/app_info.h>

#ifdef Q_OS_WIN
#include <nx/vms/client/desktop/platforms/windows/gdi_win.h>
#endif

#include "resource_browser_widget.h"
#include "layout_tab_bar.h"

using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMinimalWindowWidth = 800;
static constexpr int kMinimalWindowHeight = 600;

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

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

MainWindow::MainWindow(QnWorkbenchContext *context, QWidget *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    QnWorkbenchContextAware(context),
    m_welcomeScreen(qnRuntime->isDesktopMode() ? new QnWorkbenchWelcomeScreen(this) : nullptr),
    m_titleBar(new QnMainWindowTitleBarWidget(this, context))
{
    if (!m_welcomeScreen)
        m_welcomeScreenVisible = false;

    QnHiDpiWorkarounds::init();
#ifdef Q_OS_MACX
    // TODO: #ivigasin check the neccesarity of this line. In Maveric fullscreen animation works fine without it.
    // But with this line Mac OS shows white background in place of QGraphicsView when application enters or
    // exits fullscreen mode.
//    mac_initFullScreen((void*)winId(), (void*)this);

    // Since we have patch qt563_macos_window_level.patch we have to discard hidesOnDeactivate
    // flag for main window. Otherwise, each time it looses focus it becomes hidden.
    setHidesOnDeactivate(winId(), false);
#endif

    setAttribute(Qt::WA_AlwaysShowToolTips);
    /* And file open events on Mac. */
    installEventHandler(qApp, QEvent::FileOpen, this, &MainWindow::at_fileOpenSignalizer_activated);

    /* Set up properties. */
    setWindowTitle(QString());

    /* Initialize animations manager. */
    context->instance<workbench::Animations>();

    if (!qnRuntime->isVideoWallMode())
    {
        const bool smallWindow = qnRuntime->lightMode().testFlag(Qn::LightModeSmallWindow);
        setMinimumWidth(smallWindow ? kMinimalWindowWidth / 2 : kMinimalWindowWidth);
        setMinimumHeight(smallWindow ? kMinimalWindowHeight / 2 : kMinimalWindowHeight);
    }

    /* Set up scene & view. */
    m_scene.reset(new QnGraphicsScene(this));

    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this, [this]() {
        if (QnWorkbenchLayout *layout = workbench()->currentLayout()) {
            if (QnLayoutResourcePtr resource = layout->resource())
                disconnect(resource.data(), NULL, this, NULL);
        }
    });
    connect(workbench(), &QnWorkbench::currentLayoutChanged, this, [this]() {
        if (QnWorkbenchLayout *layout = workbench()->currentLayout()) {
            if (QnLayoutResourcePtr resource = layout->resource())
                connect(resource.data(), &QnLayoutResource::backgroundImageChanged, this, &MainWindow::updateHelpTopic);
        }
        updateHelpTopic();
    });
    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this, &MainWindow::updateHelpTopic);
    updateHelpTopic();

    m_view.reset(new QnGraphicsView(m_scene.data()));
    m_view->setAutoFillBackground(true);
    // This attribute is required to combine QGLWidget (main scene) and QQuickWidget (welcome
    // screen) in one window. Without it QGLWidget content may be not displayed in some OSes.
    m_view->setAttribute(Qt::WA_DontCreateNativeAncestors);

    /* Set up model & control machinery. */
    display()->setLightMode(qnRuntime->lightMode());
    display()->setScene(m_scene.data());
    display()->setView(m_view.data());

    if (qnRuntime->isVideoWallMode())
        display()->setNormalMarginFlags(0);
    else
        display()->setNormalMarginFlags(Qn::MarginsAffectSize | Qn::MarginsAffectPosition);

    m_controller.reset(new QnWorkbenchController(this));
    if (qnRuntime->isVideoWallMode())
        m_controller->setMenuEnabled(false);
    m_ui.reset(new WorkbenchUi(this));
    if (qnRuntime->isVideoWallMode())
        m_ui->setFlags(WorkbenchUi::HideWhenZoomed | WorkbenchUi::HideWhenNormal );
    else
        m_ui->setFlags(WorkbenchUi::HideWhenZoomed | WorkbenchUi::AdjustMargins);

    /* State manager */
    context->instance<QnWorkbenchStateManager>();

    /* Set up handlers. */
    context->instance<workbench::ActionHandler>();
    context->instance<QnWorkbenchConnectHandler>();
    context->instance<QnWorkbenchNotificationsHandler>();
    context->instance<QnWorkbenchScreenshotHandler>();
    context->instance<WorkbenchExportHandler>();
    context->instance<workbench::LayoutsHandler>();
    context->instance<PermissionsHandler>();
    context->instance<QnWorkbenchPtzHandler>();
    context->instance<QnWorkbenchDebugHandler>();
    context->instance<QnWorkbenchVideoWallHandler>();
    context->instance<QnWorkbenchWebPageHandler>();
    context->instance<QnWorkbenchIncompatibleServersActionHandler>();
    context->instance<QnWorkbenchResourcesSettingsHandler>();
    context->instance<QnWorkbenchBookmarksHandler>();
    context->instance<WorkbenchManualDeviceAdditionHandler>();
    context->instance<QnWorkbenchAlarmLayoutHandler>();
    context->instance<QnWorkbenchTextOverlaysHandler>();
    context->instance<QnWorkbenchCloudHandler>();
    context->instance<QnWorkbenchWearableHandler>();
    context->instance<workbench::LayoutToursHandler>();
    context->instance<RadassActionHandler>();
    context->instance<StartupActionsHandler>();
    context->instance<AnalyticsMenuActionsHandler>();

    context->instance<QnWorkbenchLayoutAspectRatioWatcher>();
    context->instance<QnWorkbenchPtzDialogWatcher>();

    context->instance<QnWorkbenchResourcesChangesWatcher>();
    context->instance<QnWorkbenchServerSafemodeWatcher>();
    context->instance<QnWorkbenchBookmarkTagsWatcher>();
    context->instance<QnWorkbenchItemBookmarksWatcher>();
    context->instance<QnWorkbenchBookmarksWatcher>();
    context->instance<QnTimelineBookmarksWatcher>();
    context->instance<QnWorkbenchServerPortWatcher>();
    context->instance<QnWorkbenchScreenRecordingHandler>();

    /* Set up watchers. */
    context->instance<QnWorkbenchUserInactivityWatcher>()->setMainWindow(this);
    context->instance<WorkbenchProgressManager>();

    const auto timeWatcher = context->instance<nx::vms::client::core::ServerTimeWatcher>();
    const auto timeModeNotifier = qnSettings->notifier(QnClientSettings::TIME_MODE);
    connect(timeModeNotifier, &QnPropertyNotifier::valueChanged, timeWatcher,
        [timeWatcher]()
        {
            const auto newMode = qnSettings->timeMode() == Qn::ClientTimeMode
                ? nx::vms::client::core::ServerTimeWatcher::clientTimeMode
                : nx::vms::client::core::ServerTimeWatcher::serverTimeMode;
            timeWatcher->setTimeMode(newMode);
        });

    /* Set up actions. Only these actions will be available through hotkeys. */
    addAction(action(action::NextLayoutAction));
    addAction(action(action::PreviousLayoutAction));
    addAction(action(action::SaveCurrentLayoutAction));
    addAction(action(action::SaveCurrentLayoutAsAction));
    addAction(action(action::SaveCurrentVideoWallReviewAction));
    addAction(action(action::ExitAction));
    addAction(action(action::EscapeHotkeyAction));
    addAction(action(action::FullscreenMaximizeHotkeyAction));
    addAction(action(action::AboutAction));
    addAction(action(action::PreferencesGeneralTabAction));
    addAction(action(action::OpenBookmarksSearchAction));
    addAction(action(action::OpenBusinessLogAction));
    addAction(action(action::CameraListAction));
    addAction(action(action::BusinessEventsAction));
    addAction(action(action::OpenFileAction));
    addAction(action(action::OpenNewTabAction));
    addAction(action(action::OpenNewWindowAction));
    addAction(action(action::CloseLayoutAction));
    addAction(action(action::MainMenuAction));
    addAction(action(action::OpenLoginDialogAction));
    addAction(action(action::DisconnectAction));
    addAction(action(action::OpenInFolderAction));
    addAction(action(action::RemoveLayoutItemAction));
    addAction(action(action::RemoveLayoutItemFromSceneAction));
    addAction(action(action::RemoveFromServerAction));
    addAction(action(action::StopSharingLayoutAction));
    addAction(action(action::DeleteVideoWallItemAction));
    addAction(action(action::DeleteVideowallMatrixAction));
    addAction(action(action::RemoveLayoutTourAction));
    addAction(action(action::SelectAllAction));
    addAction(action(action::CheckFileSignatureAction));
    addAction(action(action::TakeScreenshotAction));
    addAction(action(action::AdjustVideoAction));
    addAction(action(action::TogglePanicModeAction));
    addAction(action(action::ToggleLayoutTourModeAction));
    addAction(action(action::DebugIncrementCounterAction));
    addAction(action(action::DebugDecrementCounterAction));
    addAction(action(action::DebugControlPanelAction));
    addAction(action(action::SystemAdministrationAction));
    if (auto screenRecordingAction = action(action::ToggleScreenRecordingAction))
        addAction(screenRecordingAction);
    addAction(action(action::ShowFpsAction));
    addAction(action(action::OpenNewSceneAction));

    connect(action(action::MaximizeAction), &QAction::toggled, this,
        &MainWindow::setMaximized);
    connect(action(action::FullscreenAction), &QAction::toggled, this,
        &MainWindow::setFullScreen);
    connect(action(action::MinimizeAction), &QAction::triggered, this,
        &QWidget::showMinimized);
    connect(action(action::FullscreenMaximizeHotkeyAction), &QAction::triggered,
        action(action::EffectiveMaximizeAction), &QAction::trigger);

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

    m_viewLayout->addWidget(m_view.data());

    if (m_welcomeScreen)
        m_viewLayout->addWidget(m_welcomeScreen);

    // Post-initialize.
    if (nx::utils::AppInfo::isMacOsX())
        setOptions(Options());
    else
        setOptions(TitleBarDraggable);

    // Initialize system-wide menu
    if (nx::utils::AppInfo::isMacOsX())
        menu()->newMenu(action::MainScope);

    if (!qnRuntime->isAcsMode() && !qnRuntime->isProfilerMode())
    {
        /* VSync workaround must always be enabled to limit fps usage in following cases:
         * * VSync is not supported by drivers
         * * VSync is disabled in drivers
         * * double buffering is disabled in drivers or in our program
         * Workaround must be disabled in activeX mode.
         */
        auto vsyncWorkaround = new QnVSyncWorkaround(m_view->viewport(), this);
        Q_UNUSED(vsyncWorkaround);
    }

    updateWidgetsVisibility();
}

MainWindow::~MainWindow()
{
}

QWidget *MainWindow::viewport() const {
    return m_view->viewport();
}

QnWorkbenchWelcomeScreen* MainWindow::welcomeScreen() const
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

    if (m_welcomeScreen && m_welcomeScreenVisible)
        m_viewLayout->setCurrentWidget(m_welcomeScreen);
    else
        m_viewLayout->setCurrentWidget(m_view.data());

    // Always show title bar for welcome screen (it does not matter if it is fullscreen).
    m_titleBar->setVisible(isTitleVisible());

    /* Fix scene activation state (Qt bug workaround) */
    if (m_welcomeScreen && !m_welcomeScreenVisible)
    {
        /*
         * Fixes VMS-2413. The bug is following:
         * QGraphicsScene contains activation counter.
         * On WindowActivate counter is increased, on WindowsDeactivate (focus change, hide, etc) - decreased.
         * There is scenario when WindowDeactivate is called twice (change focus to 'Reconnecting' dialog,
         * then display Welcome Screen. In this case counter goes below zero, and the scene goes crazy.
         * That's why I hate constructions like:
         *   if (!--d->activationRefCount) { ... }
         * --gdm
         */
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

    updateWidgetsVisibility();
}

void MainWindow::setMaximized(bool maximized) {
    if(maximized == isMaximized())
        return;

    if(maximized) {
        showMaximized();
    } else if(isMaximized()) {
        showNormal();
    }
}

void MainWindow::setFullScreen(bool fullScreen) {
    if(fullScreen == isFullScreen())
        return;

    /*
     * Animated minimize/maximize process starts event loop,
     * so we can spoil m_storedGeometry value if enter
     * this method while already in animation progress.
     */
    if (m_inFullscreenTransition)
        return;
    QScopedValueRollback<bool> guard(m_inFullscreenTransition, true);


    if(fullScreen) {
#ifndef Q_OS_MACX
        m_storedGeometry = geometry();
#endif
        showFullScreen();
    } else if(isFullScreen()) {
        showNormal();
#ifndef Q_OS_MACX
        setGeometry(m_storedGeometry);
#endif
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
    action(action::MinimizeAction)->setEnabled(false);
#else
    QnEmulatedFrameWidget::showFullScreen();
#endif
}

void MainWindow::showNormal() {
#if defined Q_OS_MACX
    mac_showFullScreen((void*)winId(), false);
    updateDecorationsState();
    // We have to enable minimize button in Mac OS for application in non-fullscreen mode only
    action(action::MinimizeAction)->setEnabled(true);
#else
    QnEmulatedFrameWidget::showNormal();
#endif
}

void MainWindow::updateScreenInfo() {
    context()->instance<QnScreenManager>()->updateCurrentScreens(this);
}

std::pair<int, bool> MainWindow::calculateHelpTopic() const
{
    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        return {Qn::MainWindow_Scene_TourInProgress_Help, true};

    if (auto layout = workbench()->currentLayout())
    {
        if (!layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
            return {Qn::Videowall_Appearance_Help, false};

        if (layout->isSearchLayout())
            return {Qn::MainWindow_Scene_PreviewSearch_Help, true};

        if (QnLayoutResourcePtr resource = layout->resource())
        {
            if (resource->isFile())
                return{Qn::MainWindow_Tree_MultiVideo_Help, true};

            if (!resource->backgroundImageFilename().isEmpty())
                return {Qn::MainWindow_Scene_EMapping_Help, false};
        }
    }
    return {Qn::MainWindow_Scene_Help, false};
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
    const auto files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    const auto resources = QnFileProcessor::createResourcesForFiles(
        QnFileProcessor::findAcceptedFiles(files));

    if (resources.isEmpty())
        return false;

    menu()->trigger(action::DropResourcesAction, resources);
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

    action(action::FullscreenAction)->setChecked(fullScreen);
    action(action::MaximizeAction)->setChecked(maximized);

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

    const bool isTourRunning = action(action::ToggleLayoutTourModeAction)->isChecked();

    if (!isTourRunning)
    {
        if (key == Qt::Key_Space)
        {
            menu()->triggerIfPossible(action::PlayPauseAction,
                navigator()->currentParameters(action::TimelineScope));
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
            menu()->trigger(action::PreviousLayoutAction);
            break;

        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
            menu()->trigger(action::NextLayoutAction);
            break;

        default:
            // Stop layout tour if it is running.
            menu()->trigger(action::ToggleLayoutTourModeAction);
            break;
    }
    return true;
}

void MainWindow::updateContentsMargins()
{
    m_drawCustomFrame = false;
    m_frameMargins = QMargins{};

    if (nx::utils::AppInfo::isLinux() && !isFullScreen() && !isMaximized())
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
    bool result = base_type::event(event);

    if (event->type() == QEvent::WindowActivate)
    {
        // Workaround for QTBUG-34414
        if (m_welcomeScreen && m_welcomeScreenVisible)
        {
            activateWindow();
            m_welcomeScreen->activateView();
        }
    }
    else if (event->type() == QnEvent::WinSystemMenu)
    {
        action(action::MainMenuAction)->trigger();
    }

    return result;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    menu()->trigger(action::ExitAction);
}

void MainWindow::changeEvent(QEvent *event) {
    if(event->type() == QEvent::WindowStateChange)
        updateDecorationsState();

    base_type::changeEvent(event);
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    base_type::paintEvent(event);

    if (m_drawCustomFrame)
    {
        QPainter painter(this);

        painter.setPen(QPen(QColor(255, 255, 255, 64), 1));
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
    if (isFullScreen() && !isTitleVisible())
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
        qnWarning("Expected event of type %1, received an event of type %2.",
            static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    const auto fileEvent = static_cast<QFileOpenEvent *>(event);
    const auto url = fileEvent->url();
    if (!url.isEmpty() && !url.isLocalFile())
        vms::client::SelfUpdater::runNewClient(QStringList() << url.toString());
    else
        handleOpenFile(fileEvent->file());
}

#ifdef Q_OS_WIN

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    const auto msg = static_cast<MSG*>(message);
    switch (msg->message)
    {
        // Under Windows, fullscreen OpenGL widgets on the primary screen cause Windows DWM to
        // turn desktop composition off and enter fullscreen mode. To avoid this behavior,
        // an artificial delta is added to make the client area differ from the screen dimensions.
        case WM_NCCALCSIZE:
        {
            if (!isFullScreen() || isMinimized())
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
            if (!isFullScreen())
                return false;

            *result = TRUE;
            return true;
        }

        // Filling non-client area in fullscreen with the background color also improves visual
        // behavior.
        case WM_NCPAINT:
        {
            if (!isFullScreen() && !isMinimized())
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
