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
#include <QtWidgets/QStackedWidget>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>
#include <nx/vms/discovery/manager.h>

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
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <ui/workbench/workbench_welcome_screen.h>
#include <ui/workbench/handlers/workbench_action_handler.h>
#include <ui/workbench/handlers/workbench_bookmarks_handler.h>
#include <ui/workbench/handlers/workbench_connect_handler.h>
#include <ui/workbench/handlers/workbench_layouts_handler.h>
#include <ui/workbench/handlers/workbench_screenshot_handler.h>
#include <ui/workbench/handlers/workbench_export_handler.h>
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
#include <nx/client/desktop/radass/radass_action_handler.h>
#include <ui/workbench/handlers/workbench_analytics_handler.h>

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
#include <ui/workbench/workbench_ui.h>
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

#include <client_core/client_core_module.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/screen_manager.h>

#include <nx/client/desktop/ui/workbench/handlers/layout_tours_handler.h>
#include <nx/utils/app_info.h>

#include "resource_browser_widget.h"
#include "layout_tab_bar.h"
#include "dwm.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace
{
    void processWidgetsRecursively(QLayout *layout, std::function<void(QWidget*)> func)
    {
        for (int i = 0, count = layout->count(); i < count; i++)
        {
            QLayoutItem *item = layout->itemAt(i);
            if (item->widget())
                func(item->widget());
            else if (item->layout())
                processWidgetsRecursively(item->layout(), func);
        }
    }

    int minimalWindowWidth = 800;
    int minimalWindowHeight = 600;

} // anonymous namespace

#ifdef Q_OS_MACX
extern "C" {
    void disable_animations(void *qnmainwindow) {
        MainWindow* mainwindow = (MainWindow*)qnmainwindow;
        mainwindow->setAnimationsEnabled(false);
    }

    void enable_animations(void *qnmainwindow) {
        MainWindow* mainwindow = (MainWindow*)qnmainwindow;
        mainwindow->setAnimationsEnabled(true);
    }
}
#endif

MainWindow::MainWindow(QnWorkbenchContext *context, QWidget *parent, Qt::WindowFlags flags) :
    base_type(parent, flags | Qt::Window | Qt::CustomizeWindowHint
#ifdef Q_OS_MACX
        | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint
#endif
        ),
    QnWorkbenchContextAware(context),
    m_dwm(nullptr),
    m_welcomeScreen(
        qnRuntime->isDesktopMode()
            ? new QnWorkbenchWelcomeScreen(qnClientCoreModule->mainQmlEngine(), this)
            : nullptr),
    m_currentPageHolder(new QStackedWidget(this)),
    m_titleBar(new QnMainWindowTitleBarWidget(this)),
    m_titleVisible(true),
    m_drawCustomFrame(false),
    m_inFullscreenTransition(false)
{
    QnHiDpiWorkarounds::init();
#ifdef Q_OS_MACX
    // TODO: #ivigasin check the neccesarity of this line. In Maveric fullscreen animation works fine without it.
    // But with this line Mac OS shows white background in place of QGraphicsView when application enters or
    // exits fullscreen mode.
//    mac_initFullScreen((void*)winId(), (void*)this);
#endif

    setAttribute(Qt::WA_AlwaysShowToolTips);

    /* And file open events on Mac. */
    installEventHandler(qApp, QEvent::FileOpen, this, &MainWindow::at_fileOpenSignalizer_activated);

    /* Set up dwm. */
    m_dwm = new QnDwm(this);

    connect(m_dwm, &QnDwm::compositionChanged, this, &MainWindow::updateDwmState);

    /* Set up properties. */
    setWindowTitle(QString());

    /* Initialize animations manager. */
    context->instance<workbench::Animations>();

    if (!qnRuntime->isVideoWallMode()) {
        bool smallWindow = qnSettings->lightMode() & Qn::LightModeSmallWindow;
        setMinimumWidth(smallWindow ? minimalWindowWidth / 2 : minimalWindowWidth);
        setMinimumHeight(smallWindow ? minimalWindowHeight / 2 : minimalWindowHeight);
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

    /* Set up model & control machinery. */
    display()->setLightMode(qnSettings->lightMode());
    display()->setScene(m_scene.data());
    display()->setView(m_view.data());

    if (qnRuntime->isVideoWallMode())
        display()->setNormalMarginFlags(0);
    else
        display()->setNormalMarginFlags(Qn::MarginsAffectSize | Qn::MarginsAffectPosition);

    m_controller.reset(new QnWorkbenchController(this));
    if (qnRuntime->isVideoWallMode())
        m_controller->setMenuEnabled(false);
    m_ui.reset(new QnWorkbenchUi(this));
    if (qnRuntime->isVideoWallMode())
        m_ui->setFlags(QnWorkbenchUi::HideWhenZoomed | QnWorkbenchUi::HideWhenNormal );
    else
        m_ui->setFlags(QnWorkbenchUi::HideWhenZoomed | QnWorkbenchUi::AdjustMargins);

    /* State manager */
    context->instance<QnWorkbenchStateManager>();

    /* Set up handlers. */
    context->instance<workbench::ActionHandler>();
    context->instance<QnWorkbenchConnectHandler>();
    context->instance<QnWorkbenchNotificationsHandler>();
    context->instance<QnWorkbenchScreenshotHandler>();
    context->instance<QnWorkbenchExportHandler>();
    context->instance<workbench::LayoutsHandler>();
    context->instance<QnWorkbenchPtzHandler>();
    context->instance<QnWorkbenchDebugHandler>();
    context->instance<QnWorkbenchVideoWallHandler>();
    context->instance<QnWorkbenchWebPageHandler>();
    context->instance<QnWorkbenchIncompatibleServersActionHandler>();
    context->instance<QnWorkbenchResourcesSettingsHandler>();
    context->instance<QnWorkbenchBookmarksHandler>();
    context->instance<QnWorkbenchAlarmLayoutHandler>();
    context->instance<QnWorkbenchTextOverlaysHandler>();
    context->instance<QnWorkbenchCloudHandler>();
    context->instance<workbench::LayoutToursHandler>();
    context->instance<nx::client::desktop::RadassActionHandler>();
    context->instance<QnWorkbenchAnalyticsHandler>();

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

    m_viewLayout = new QVBoxLayout();
    m_viewLayout->setContentsMargins(0, 0, 0, 0);
    m_viewLayout->setSpacing(0);
    m_viewLayout->addWidget(m_currentPageHolder);

    m_globalLayout = new QVBoxLayout();
    m_globalLayout->setContentsMargins(0, 0, 0, 0);
    m_globalLayout->setSpacing(0);

    m_globalLayout->addWidget(m_titleBar);
    m_globalLayout->addLayout(m_viewLayout);
    m_globalLayout->setStretchFactor(m_viewLayout, 0x1000);

    setLayout(m_globalLayout);

    if (qnRuntime->isDesktopMode())
        m_currentPageHolder->addWidget(new QWidget());

    m_currentPageHolder->addWidget(m_view.data());

    if (qnRuntime->isDesktopMode())
        m_currentPageHolder->addWidget(m_welcomeScreen);

    // Post-initialize.
    if (nx::utils::AppInfo::isMacOsX())
        setOptions(Options());
    else
        setOptions(TitleBarDraggable);

    // Initialize system-wide menu
    if (nx::utils::AppInfo::isMacOsX())
        menu()->newMenu(action::MainScope);

    if (!qnRuntime->isActiveXMode())
    {
        /* VSync workaround must always be enabled to limit fps usage in following cases:
         * * VSync is not supported by drivers
         * * VSync is disabled in drivers
         * * double buffering is disabled in drivers or in our program
         * Workaround must be disabled in activeX mode.
         */
         QnVSyncWorkaround *vsyncWorkaround = new QnVSyncWorkaround(m_view->viewport(), this);
         Q_UNUSED(vsyncWorkaround);
    }

    updateWidgetsVisibility();
}

MainWindow::~MainWindow()
{
    m_dwm = NULL;
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

    if (m_welcomeScreenVisible)
        m_currentPageHolder->setCurrentWidget(m_welcomeScreen);
    else
        m_currentPageHolder->setCurrentWidget(m_view.data());

    // Always show title bar for welcome screen (it does not matter if it is fullscreen).
    m_titleBar->setVisible(isTitleVisible());

    /* Fix scene activation state (Qt bug workaround) */
    if (!m_welcomeScreenVisible)
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

    updateDwmState();
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
    QN_SCOPED_VALUE_ROLLBACK(&m_inFullscreenTransition, true);


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

bool MainWindow::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);

    QnResourceList resources = QnFileProcessor::createResourcesForFiles(
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

void MainWindow::updateDecorationsState() {
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

    bool windowTitleUsed = !uiTitleUsed && !qnRuntime->isVideoWallMode() && !qnRuntime->isActiveXMode();
    setTitleVisible(windowTitleUsed);
    m_ui->setTitleUsed(uiTitleUsed && !qnRuntime->isVideoWallMode() && !qnRuntime->isActiveXMode());
    m_view->setLineWidth(windowTitleUsed ? 0 : 1);

    updateDwmState();
    m_currentPageHolder->updateGeometry();
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
        {
            menu()->trigger(action::PreviousLayoutAction);
            break;
        }
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        {
            menu()->trigger(action::NextLayoutAction);
            break;
        }
        default:
            // Stop layout tour if it is running.
            menu()->trigger(action::ToggleLayoutTourModeAction);
            break;
    }
    return true;
}

void MainWindow::updateDwmState()
{
    if (isFullScreen())
    {
        /* Full screen mode. */
        m_drawCustomFrame = false;
        m_frameMargins = QMargins(0, 0, 0, 0);

        if (m_dwm->isSupported() && false)
        { // TODO: Disable DWM for now.
            setAttribute(Qt::WA_NoSystemBackground, false);
            setAttribute(Qt::WA_TranslucentBackground, false);

            m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
            m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
            m_dwm->disableBlurBehindWindow();
        }

        /* Can't set to (0, 0, 0, 0) on Windows as in fullScreen mode context menu becomes invisible.
         * Looks like Qt bug: https://bugreports.qt.io/browse/QTBUG-7556 */
#ifdef Q_OS_WIN
        // TODO: #vkutin #GDM Mouse in the leftmost pixel doesn't trigger autohidden workbench tree show
        setContentsMargins(1, 0, 0, 0); //FIXME
#else
        setContentsMargins(0, 0, 0, 0);
#endif

        m_viewLayout->setContentsMargins(0, 0, 0, 0);
    }
    else if (m_dwm->isSupported() && m_dwm->isCompositionEnabled() && false)
    { // TODO: Disable DWM for now.
        /* Windowed or maximized with aero glass. */
        m_drawCustomFrame = false;
        m_frameMargins = !isMaximized() ? m_dwm->themeFrameMargins() : QMargins(0, 0, 0, 0);

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);

        m_dwm->extendFrameIntoClientArea();
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->enableBlurBehindWindow();

        setContentsMargins(0, 0, 0, 0);

        m_viewLayout->setContentsMargins(
            m_frameMargins.left(),
            isTitleVisible() ? 0 : m_frameMargins.top(),
            m_frameMargins.right(),
            m_frameMargins.bottom()
        );
    }
    else
    {
        /* Windowed or maximized without aero glass. */
#ifdef Q_OS_LINUX
        // On linux window manager cannot disable titlebar leaving border in place. Thus we have to disable decorations completely and draw our own border.
        if (isMaximized())
        {
            m_drawCustomFrame = false;
            m_frameMargins = QMargins(0, 0, 0, 0);
        }
        else
        {
            m_drawCustomFrame = true;
            m_frameMargins = QMargins(2, 2, 2, 2);
        }
#else
        m_drawCustomFrame = false;
        m_frameMargins = QMargins(0, 0, 0, 0);
#endif

        if(m_dwm->isSupported() && false)
        { // TODO: Disable DWM for now.
            setAttribute(Qt::WA_NoSystemBackground, false);
            setAttribute(Qt::WA_TranslucentBackground, false);

            m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
            m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
            m_dwm->disableBlurBehindWindow();
        }

        setContentsMargins(0, 0, 0, 0);

        m_viewLayout->setContentsMargins(
            m_frameMargins.left(),
            isTitleVisible() ? 0 : m_frameMargins.top(),
            m_frameMargins.right(),
            m_frameMargins.bottom()
        );
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool MainWindow::event(QEvent* event)
{
    bool result = base_type::event(event);

    if (event->type() == QEvent::WindowActivate)
    {
        if (m_welcomeScreen && m_welcomeScreenVisible)
        {
            // Welcome screen looses focus after window deactivation. We restore it here.
            m_welcomeScreen->forceActiveFocus();
        }
    }
    else if (event->type() == QnEvent::WinSystemMenu)
    {
        action(action::MainMenuAction)->trigger();
    }

    if (m_dwm)
        result |= m_dwm->widgetEvent(event);

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

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
    /* Note that we may get here from destructor, so check for dwm is needed. */
    if(m_dwm && m_dwm->widgetNativeEvent(eventType, message, result))
        return true;

    return base_type::nativeEvent(eventType, message, result);
}

Qt::WindowFrameSection MainWindow::windowFrameSectionAt(const QPoint &pos) const
{
    if (isFullScreen() && !isTitleVisible())
        return Qt::NoSection;

    Qt::WindowFrameSection result = Qn::toNaturalQtFrameSection(
            Qn::calculateRectangularFrameSections(
                    rect(),
                    QnGeometry::eroded(rect(), m_frameMargins),
                    QRect(pos, pos)));

    if (m_options.testFlag(TitleBarDraggable) &&
        result == Qt::NoSection &&
        pos.y() <= m_titleBar->mapTo(this, m_titleBar->rect().bottomRight()).y())
    {
        result = Qt::TitleBarArea;
    }

    return result;
}

void MainWindow::at_fileOpenSignalizer_activated(QObject *, QEvent *event) {
    if(event->type() != QEvent::FileOpen) {
        qnWarning("Expected event of type %1, received an event of type %2.", static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    handleMessage(static_cast<QFileOpenEvent *>(event)->file());
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
