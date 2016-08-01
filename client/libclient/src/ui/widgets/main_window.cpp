#include "main_window.h"

#ifdef Q_OS_MACX
#include <ui/workaround/mac_utils.h>
#endif

#include <QtCore/QFile>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QFileOpenEvent>
#include <QtNetwork/QNetworkReply>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>
#include <network/module_finder.h>

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
#include <ui/actions/action_manager.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

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

#include <ui/workbench/watchers/workbench_user_inactivity_watcher.h>
#include <ui/workbench/watchers/workbench_layout_aspect_ratio_watcher.h>
#include <ui/workbench/watchers/workbench_ptz_dialog_watcher.h>
#include <ui/workbench/watchers/workbench_system_name_watcher.h>
#include <ui/workbench/watchers/workbench_server_address_watcher.h>
#include <ui/workbench/watchers/workbench_server_port_watcher.h>
#include <ui/workbench/watchers/workbench_resources_changes_watcher.h>
#include <ui/workbench/watchers/workbench_server_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_bookmark_tags_watcher.h>
#include <ui/workbench/watchers/workbench_bookmarks_watcher.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/watchers/current_user_available_cameras_watcher.h>
#include <ui/workbench/watchers/workbench_item_bookmarks_watcher.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_ui.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_context.h>

#include <ui/widgets/main_window_title_bar_widget.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/workaround/vsync_workaround.h>
#include <ui/screen_recording/screen_recorder.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_message_processor.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/screen_manager.h>

#include "resource_browser_widget.h"
#include "layout_tab_bar.h"
#include "dwm.h"

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
        QnMainWindow* mainwindow = (QnMainWindow*)qnmainwindow;
        mainwindow->setAnimationsEnabled(false);
    }

    void enable_animations(void *qnmainwindow) {
        QnMainWindow* mainwindow = (QnMainWindow*)qnmainwindow;
        mainwindow->setAnimationsEnabled(true);
    }
}
#endif

QnMainWindow::QnMainWindow(QnWorkbenchContext *context, QWidget *parent, Qt::WindowFlags flags) :
    base_type(parent, flags | Qt::Window | Qt::CustomizeWindowHint
#ifdef Q_OS_MACX
        | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint
#endif
        ),
    QnWorkbenchContextAware(context),
    m_dwm(nullptr),
    m_currentPageHolder(new QStackedWidget()),
    m_titleBar(new QnMainWindowTitleBarWidget(this)),
    m_titleVisible(true),
    m_drawCustomFrame(false),
    m_inFullscreenTransition(false)
{
#ifdef Q_OS_MACX
    // TODO: #ivigasin check the neccesarity of this line. In Maveric fullscreen animation works fine without it.
    // But with this line Mac OS shows white background in place of QGraphicsView when application enters or
    // exits fullscreen mode.
//    mac_initFullScreen((void*)winId(), (void*)this);
#endif

    setAttribute(Qt::WA_AlwaysShowToolTips);

    /* And file open events on Mac. */
    QnSingleEventSignalizer *fileOpenSignalizer = new QnSingleEventSignalizer(this);
    fileOpenSignalizer->setEventType(QEvent::FileOpen);
    qApp->installEventFilter(fileOpenSignalizer);
    connect(fileOpenSignalizer,             SIGNAL(activated(QObject *, QEvent *)),         this,                                   SLOT(at_fileOpenSignalizer_activated(QObject *, QEvent *)));

    /* Set up dwm. */
    m_dwm = new QnDwm(this);

    connect(m_dwm,                          SIGNAL(compositionChanged()),                   this,                                   SLOT(updateDwmState()));

    /* Set up properties. */
    setWindowTitle(QString());

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
                connect(resource.data(), &QnLayoutResource::backgroundImageChanged, this, &QnMainWindow::updateHelpTopic);
        }
        updateHelpTopic();
    });
    connect(action(QnActions::ToggleTourModeAction), &QAction::toggled, this, &QnMainWindow::updateHelpTopic);
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
    context->instance<QnWorkbenchActionHandler>();
    context->instance<QnWorkbenchConnectHandler>();
    context->instance<QnWorkbenchNotificationsHandler>();
    context->instance<QnWorkbenchScreenshotHandler>();
    context->instance<QnWorkbenchExportHandler>();
    context->instance<QnWorkbenchLayoutsHandler>();
    context->instance<QnWorkbenchPtzHandler>();
#ifdef _DEBUG
    context->instance<QnWorkbenchDebugHandler>();
#endif
    context->instance<QnWorkbenchVideoWallHandler>();
    context->instance<QnWorkbenchWebPageHandler>();
    context->instance<QnWorkbenchIncompatibleServersActionHandler>();
    context->instance<QnWorkbenchResourcesSettingsHandler>();
    context->instance<QnWorkbenchBookmarksHandler>();
    context->instance<QnWorkbenchAlarmLayoutHandler>();
    context->instance<QnWorkbenchCloudHandler>();

    context->instance<QnWorkbenchLayoutAspectRatioWatcher>();
    context->instance<QnWorkbenchPtzDialogWatcher>();
    context->instance<QnWorkbenchSystemNameWatcher>();
    context->instance<QnWorkbenchServerAddressWatcher>();
    context->instance<QnWorkbenchResourcesChangesWatcher>();
    context->instance<QnWorkbenchServerSafemodeWatcher>();
    context->instance<QnWorkbenchBookmarkTagsWatcher>();
    context->instance<QnWorkbenchItemBookmarksWatcher>();
    context->instance<QnWorkbenchBookmarksWatcher>();
    context->instance<QnTimelineBookmarksWatcher>();
    context->instance<QnWorkbenchServerPortWatcher>();
    context->instance<QnCurrentUserAvailableCamerasWatcher>();

    /* Set up watchers. */
    context->instance<QnWorkbenchUserInactivityWatcher>()->setMainWindow(this);

    /* Set up actions. Only these actions will be available through hotkeys. */
    addAction(action(QnActions::NextLayoutAction));
    addAction(action(QnActions::PreviousLayoutAction));
    addAction(action(QnActions::SaveCurrentLayoutAction));
    addAction(action(QnActions::SaveCurrentLayoutAsAction));
    addAction(action(QnActions::SaveCurrentVideoWallReviewAction));
    addAction(action(QnActions::ExitAction));
    addAction(action(QnActions::EscapeHotkeyAction));
    addAction(action(QnActions::FullscreenMaximizeHotkeyAction));
    addAction(action(QnActions::AboutAction));
    addAction(action(QnActions::PreferencesGeneralTabAction));
    addAction(action(QnActions::OpenBookmarksSearchAction));
    addAction(action(QnActions::OpenBusinessLogAction));
    addAction(action(QnActions::CameraListAction));
    addAction(action(QnActions::BusinessEventsAction));
    addAction(action(QnActions::OpenFileAction));
    addAction(action(QnActions::OpenNewTabAction));
    addAction(action(QnActions::OpenNewWindowAction));
    addAction(action(QnActions::CloseLayoutAction));
    addAction(action(QnActions::MainMenuAction));
    addAction(action(QnActions::OpenLoginDialogAction));
    addAction(action(QnActions::OpenInFolderAction));
    addAction(action(QnActions::RemoveLayoutItemAction));
    addAction(action(QnActions::RemoveFromServerAction));
    addAction(action(QnActions::StopSharingLayoutAction));
    addAction(action(QnActions::DeleteVideoWallItemAction));
    addAction(action(QnActions::DeleteVideowallMatrixAction));
    addAction(action(QnActions::SelectAllAction));
    addAction(action(QnActions::CheckFileSignatureAction));
    addAction(action(QnActions::TakeScreenshotAction));
    addAction(action(QnActions::AdjustVideoAction));
    addAction(action(QnActions::TogglePanicModeAction));
    addAction(action(QnActions::ToggleTourModeAction));
    addAction(action(QnActions::DebugIncrementCounterAction));
    addAction(action(QnActions::DebugDecrementCounterAction));
    addAction(action(QnActions::DebugShowResourcePoolAction));
    addAction(action(QnActions::DebugControlPanelAction));
    addAction(action(QnActions::SystemAdministrationAction));

    connect(action(QnActions::MaximizeAction),     SIGNAL(toggled(bool)),                          this,                                   SLOT(setMaximized(bool)));
    connect(action(QnActions::FullscreenAction),   SIGNAL(toggled(bool)),                          this,                                   SLOT(setFullScreen(bool)));
    connect(action(QnActions::MinimizeAction),     SIGNAL(triggered()),                            this,                                   SLOT(minimize()));
    connect(action(QnActions::FullscreenMaximizeHotkeyAction), SIGNAL(triggered()),                action(QnActions::EffectiveMaximizeAction),    SLOT(trigger()));

    menu()->setTargetProvider(m_ui.data());

    const auto welcomeScreen = context->instance<QnWorkbenchWelcomeScreen>();
    connect(welcomeScreen, &QnWorkbenchWelcomeScreen::visibleChanged,
        this, &QnMainWindow::updateWidgetsVisibility);

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

    m_currentPageHolder->addWidget(new QWidget());
    m_currentPageHolder->addWidget(m_view.data());
    m_currentPageHolder->addWidget(welcomeScreen->widget());

/* Post-initialize. */

#ifdef Q_OS_MACX
    setOptions(Options());
#else
    setOptions(TitleBarDraggable);
#endif

    /* Open single tab. */
    action(QnActions::OpenNewTabAction)->trigger();

#ifdef Q_OS_MACX
    //initialize system-wide menu
    menu()->newMenu(Qn::MainScope);
#endif

    /* VSync workaround must always be enabled to limit fps usage in following cases:
     * * VSync is not supported by drivers
     * * VSync is disabled in drivers
     * * double buffering is disabled in drivers or in our program
     */
    QnVSyncWorkaround *vsyncWorkaround = new QnVSyncWorkaround(m_view->viewport(), this);
    Q_UNUSED(vsyncWorkaround);

    updateWidgetsVisibility();
}

QnMainWindow::~QnMainWindow() {
    m_dwm = NULL;
}

QWidget *QnMainWindow::viewport() const {
    return m_view->viewport();
}

bool QnMainWindow::isTitleVisible() const
{
    return m_titleVisible || isWelcomeScreenVisible();
}

bool QnMainWindow::isWelcomeScreenVisible() const
{
    const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
    return (welcomeScreen && welcomeScreen->isVisible());
}

void QnMainWindow::updateWidgetsVisibility()
{
    const auto updateWelcomeScreenVisibility =
        [this](bool welcomeScreenIsVisible)
    {
        enum { kWorkaroundPage, kSceneIndex, kWelcomePageIndex };

        // Due to flickering when switching between two opengl contexts
        // we have to use intermediate non-opengl page switch.
        m_currentPageHolder->setCurrentIndex(kWorkaroundPage);

        if (welcomeScreenIsVisible)
            m_titleBar->setVisible(isTitleVisible());
        m_currentPageHolder->repaint();
        if (!welcomeScreenIsVisible)
            m_titleBar->setVisible(isTitleVisible());

        m_currentPageHolder->setCurrentIndex(welcomeScreenIsVisible
            ? kWelcomePageIndex : kSceneIndex);

        /* Fix scene activation state (Qt bug workaround) */
        if (welcomeScreenIsVisible)
            return;

        if (!display() || !display()->scene())
            return;

        if (display()->scene()->isActive())
            return;

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
        QObject* sceneObject = display()->scene();
        sceneObject->event(&e);
    };

    // Always show title bar for welcome screen (it does not matter if it is fullscreen)

    m_titleBar->setTabBarStuffVisible(!isWelcomeScreenVisible());
    updateWelcomeScreenVisibility(isWelcomeScreenVisible());
    updateDwmState();
}

void QnMainWindow::setTitleVisible(bool visible)
{
    if(m_titleVisible == visible)
        return;

    m_titleVisible = visible;

    updateWidgetsVisibility();
}

void QnMainWindow::setMaximized(bool maximized) {
    if(maximized == isMaximized())
        return;

    if(maximized) {
        showMaximized();
    } else if(isMaximized()) {
        showNormal();
    }
}

void QnMainWindow::setFullScreen(bool fullScreen) {
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

void QnMainWindow::setAnimationsEnabled(bool enabled) {
    InstrumentManager *manager = InstrumentManager::instance(m_scene.data());
    manager->setAnimationsEnabled(enabled);
}

void QnMainWindow::showFullScreen() {
#if defined Q_OS_MACX
    mac_showFullScreen((void*)winId(), true);
    updateDecorationsState();
#else
    QnEmulatedFrameWidget::showFullScreen();
#endif
}

void QnMainWindow::showNormal() {
#if defined Q_OS_MACX
    mac_showFullScreen((void*)winId(), false);
    updateDecorationsState();
#else
    QnEmulatedFrameWidget::showNormal();
#endif
}

void QnMainWindow::updateScreenInfo() {
    context()->instance<QnScreenManager>()->updateCurrentScreens(this);
}

void QnMainWindow::updateHelpTopic() {
    if (action(QnActions::ToggleTourModeAction)->isChecked()) {
        setHelpTopic(m_scene.data(), Qn::MainWindow_Scene_TourInProgress_Help, true);
        return;
    }

    if (QnWorkbenchLayout *layout = workbench()->currentLayout()) {
        if (!layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull()) {
            setHelpTopic(m_scene.data(), Qn::Videowall_Appearance_Help);
            return;
        }
        if (layout->isSearchLayout()) {
            setHelpTopic(m_scene.data(), Qn::MainWindow_Scene_PreviewSearch_Help, true);
            return;
        }
        if (QnLayoutResourcePtr resource = layout->resource())
        {
            if (resource->isFile())
            {
                setHelpTopic(m_scene.data(), Qn::MainWindow_Tree_MultiVideo_Help, true);
                return;
            }
            if (!resource->backgroundImageFilename().isEmpty())
            {
                setHelpTopic(m_scene.data(), Qn::MainWindow_Scene_EMapping_Help);
                return;
            }
        }
    }
    setHelpTopic(m_scene.data(), Qn::MainWindow_Scene_Help);
}

void QnMainWindow::minimize() {
    showMinimized();
}

bool QnMainWindow::handleMessage(const QString &message) {
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);

    QnResourceList resources = QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files));
    if (resources.isEmpty())
        return false;

    menu()->trigger(QnActions::DropResourcesAction, resources);
    return true;
}

QnMainWindow::Options QnMainWindow::options() const {
    return m_options;
}

void QnMainWindow::setOptions(Options options) {
    if(m_options == options)
        return;

    m_options = options;
}

void QnMainWindow::updateDecorationsState() {
#ifdef Q_OS_MACX
    bool fullScreen = mac_isFullscreen((void*)winId());
#else
    bool fullScreen = isFullScreen();
#endif
    bool maximized = isMaximized();

    action(QnActions::FullscreenAction)->setChecked(fullScreen);
    action(QnActions::MaximizeAction)->setChecked(maximized);

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

void QnMainWindow::updateDwmState() {
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
        setContentsMargins(0, 0, 0, 1);
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
bool QnMainWindow::event(QEvent *event) {
    bool result = base_type::event(event);

    if(m_dwm != NULL)
        result |= m_dwm->widgetEvent(event);

    return result;
}

void QnMainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    menu()->trigger(QnActions::ExitAction);
}

void QnMainWindow::changeEvent(QEvent *event) {
    if(event->type() == QEvent::WindowStateChange)
        updateDecorationsState();

    base_type::changeEvent(event);
}

void QnMainWindow::paintEvent(QPaintEvent *event) {
    base_type::paintEvent(event);

    if(m_drawCustomFrame) {
        QPainter painter(this);

        painter.setPen(QPen(QColor(255, 255, 255, 64), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

void QnMainWindow::keyPressEvent(QKeyEvent *event) {
    base_type::keyPressEvent(event);

    if (!action(QnActions::ToggleTourModeAction)->isChecked())
        return;

    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_Control)
        return;
    menu()->trigger(QnActions::ToggleTourModeAction);
}

void QnMainWindow::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);
    updateScreenInfo();
}

void QnMainWindow::moveEvent(QMoveEvent *event) {
    base_type::moveEvent(event);
    updateScreenInfo();
}

bool QnMainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
    /* Note that we may get here from destructor, so check for dwm is needed. */
    if(m_dwm && m_dwm->widgetNativeEvent(eventType, message, result))
        return true;

    return base_type::nativeEvent(eventType, message, result);
}

Qt::WindowFrameSection QnMainWindow::windowFrameSectionAt(const QPoint &pos) const
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

void QnMainWindow::at_fileOpenSignalizer_activated(QObject *, QEvent *event) {
    if(event->type() != QEvent::FileOpen) {
        qnWarning("Expected event of type %1, received an event of type %2.", static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    handleMessage(static_cast<QFileOpenEvent *>(event)->file());
}
