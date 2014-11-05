#include "main_window.h"

#ifdef Q_OS_MACX
#include <ui/workaround/mac_utils.h>
#endif

#include <QtCore/QFile>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtGui/QFileOpenEvent>
#include <QtNetwork/QNetworkReply>

#include <utils/common/warnings.h>
#include <utils/common/event_processors.h>
#include <utils/common/environment.h>
#include <utils/network/module_finder.h>

#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>

#include <api/session_manager.h>

#include <ui/common/palette.h>
#include <ui/common/frame_section.h>
#include <ui/actions/action_manager.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/graphics/view/gradient_background_painter.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/dialogs/ptz_manage_dialog.h>

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
#include <ui/workbench/watchers/workbench_user_inactivity_watcher.h>
#include <ui/workbench/watchers/workbench_layout_aspect_ratio_watcher.h>
#include <ui/workbench/watchers/workbench_ptz_dialog_watcher.h>
#include <ui/workbench/watchers/workbench_system_name_watcher.h>
#include <ui/workbench/watchers/workbench_server_address_watcher.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_ui.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/style/noptix_style.h>
#include <ui/style/proxy_style.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/workaround/vsync_workaround.h>
#include <ui/screen_recording/screen_recorder.h>

#include <client/client_settings.h>

#include <utils/common/scoped_value_rollback.h>

#include "resource_browser_widget.h"
#include "layout_tab_bar.h"
#include "dwm.h"

namespace {

    QToolButton *newActionButton(QAction *action, bool popup = false, qreal sizeMultiplier = 1.0, int helpTopicId = -1) {
        QToolButton *button = new QToolButton();
        button->setDefaultAction(action);

        qreal aspectRatio = QnGeometry::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));
        int iconHeight = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, 0, button) * sizeMultiplier;
        int iconWidth = iconHeight * aspectRatio;
        button->setFixedSize(iconWidth, iconHeight);

        button->setProperty(Qn::ToolButtonCheckedRotationSpeed, action->property(Qn::ToolButtonCheckedRotationSpeed));

        if(popup) {
            /* We want the button to activate the corresponding action so that menu is updated. 
             * However, menu buttons do not activate their corresponding actions as they do not receive release events. 
             * We work this around by making some hacky connections. */
            button->setPopupMode(QToolButton::InstantPopup);

            QObject::disconnect(button, SIGNAL(pressed()),  button,                     SLOT(_q_buttonPressed()));
            QObject::connect(button,    SIGNAL(pressed()),  button->defaultAction(),    SLOT(trigger()));
            QObject::connect(button,    SIGNAL(pressed()),  button,                     SLOT(_q_buttonPressed()));
        }

        if(helpTopicId != -1)
            setHelpTopic(button, helpTopicId);

        return button;
    }

    void setVisibleRecursively(QLayout *layout, bool visible) {
        for(int i = 0, count = layout->count(); i < count; i++) {
            QLayoutItem *item = layout->itemAt(i);
            if(item->widget()) {
                item->widget()->setVisible(visible);
            } else if(item->layout()) {
                setVisibleRecursively(item->layout(), visible);
            }
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

QnMainWindow::QnMainWindow(QnWorkbenchContext *context, QWidget *parent, Qt::WindowFlags flags): 
    base_type(parent, flags | Qt::Window
#ifndef Q_OS_MACX
    | Qt::CustomizeWindowHint
#endif
    ),
    QnWorkbenchContextAware(context),
    m_controller(0),
    m_titleVisible(true),
    m_skipDoubleClick(false),
    m_dwm(NULL),
    m_drawCustomFrame(false),
    m_enableBackgroundAnimation(true),
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
    setAcceptDrops(true);

    if (!qnSettings->isVideoWallMode()) {
        bool smallWindow = qnSettings->lightMode() & Qn::LightModeSmallWindow;
        setMinimumWidth(smallWindow ? minimalWindowWidth / 2 : minimalWindowWidth);
        setMinimumHeight(smallWindow ? minimalWindowHeight / 2 : minimalWindowHeight);
    }

    /* Set up scene & view. */
    m_scene.reset(new QnGraphicsScene(this));
    setHelpTopic(m_scene.data(), Qn::MainWindow_Scene_Help);

    m_view.reset(new QnGraphicsView(m_scene.data()));
    m_view->setAutoFillBackground(true);

    /* Set up model & control machinery. */
    display()->setScene(m_scene.data());
    display()->setView(m_view.data());
    if (qnSettings->isVideoWallMode())
        display()->setNormalMarginFlags(0);
    else
        display()->setNormalMarginFlags(Qn::MarginsAffectSize | Qn::MarginsAffectPosition);

    m_controller.reset(new QnWorkbenchController(this));
    if (qnSettings->isVideoWallMode())
        m_controller->setMenuEnabled(false);
    m_ui.reset(new QnWorkbenchUi(this));
    if (qnSettings->isVideoWallMode())
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
    context->instance<QnWorkbenchDebugHandler>();
    context->instance<QnWorkbenchVideoWallHandler>();
    context->instance<QnWorkbenchIncompatibleServersActionHandler>();
#ifdef QN_ENABLE_BOOKMARKS
    context->instance<QnWorkbenchBookmarksHandler>();
#endif
    context->instance<QnWorkbenchLayoutAspectRatioWatcher>();
    context->instance<QnWorkbenchPtzDialogWatcher>();
    context->instance<QnWorkbenchSystemNameWatcher>();

    QnWorkbenchServerAddressWatcher *serverAddressWatcher = context->instance<QnWorkbenchServerAddressWatcher>();
    serverAddressWatcher->setDirectModuleFinder(QnModuleFinder::instance()->directModuleFinder());
    serverAddressWatcher->setDirectModuleFinderHelper(QnModuleFinder::instance()->directModuleFinderHelper());

    /* Set up watchers. */
    context->instance<QnWorkbenchUserInactivityWatcher>()->setMainWindow(this);

    /* Set up actions. Only these actions will be available through hotkeys. */
    addAction(action(Qn::NextLayoutAction));
    addAction(action(Qn::PreviousLayoutAction));
    addAction(action(Qn::SaveCurrentLayoutAction));
    addAction(action(Qn::SaveCurrentLayoutAsAction));
    addAction(action(Qn::SaveCurrentVideoWallReviewAction));
    addAction(action(Qn::ExitAction));
    addAction(action(Qn::EscapeHotkeyAction));
    addAction(action(Qn::FullscreenMaximizeHotkeyAction));
    addAction(action(Qn::AboutAction));
    addAction(action(Qn::PreferencesGeneralTabAction));
    addAction(action(Qn::BusinessEventsLogAction));
    addAction(action(Qn::CameraListAction));
    addAction(action(Qn::BusinessEventsAction));
    addAction(action(Qn::OpenFileAction));
    addAction(action(Qn::OpenNewTabAction));
    addAction(action(Qn::OpenNewWindowAction));
    addAction(action(Qn::CloseLayoutAction));
    addAction(action(Qn::MainMenuAction));
    addAction(action(Qn::OpenLoginDialogAction));
    addAction(action(Qn::OpenInFolderAction));
    addAction(action(Qn::RemoveLayoutItemAction));
    addAction(action(Qn::RemoveFromServerAction));
    addAction(action(Qn::DeleteVideoWallItemAction));
    addAction(action(Qn::DeleteVideowallMatrixAction));
    addAction(action(Qn::SelectAllAction));
    addAction(action(Qn::CheckFileSignatureAction));
    addAction(action(Qn::TakeScreenshotAction));
    addAction(action(Qn::AdjustVideoAction));
    addAction(action(Qn::TogglePanicModeAction));
    addAction(action(Qn::ToggleTourModeAction));
    addAction(action(Qn::DebugIncrementCounterAction));
    addAction(action(Qn::DebugDecrementCounterAction));
    addAction(action(Qn::DebugShowResourcePoolAction));
    addAction(action(Qn::DebugControlPanelAction));
    addAction(action(Qn::ToggleBackgroundAnimationAction));
#ifdef _DEBUG
    addAction(action(Qn::SystemAdministrationAction));
#endif
    connect(action(Qn::MaximizeAction),     SIGNAL(toggled(bool)),                          this,                                   SLOT(setMaximized(bool)));
    connect(action(Qn::FullscreenAction),   SIGNAL(toggled(bool)),                          this,                                   SLOT(setFullScreen(bool)));
    connect(action(Qn::MinimizeAction),     SIGNAL(triggered()),                            this,                                   SLOT(minimize()));
    connect(action(Qn::FullscreenMaximizeHotkeyAction), SIGNAL(triggered()),                action(Qn::EffectiveMaximizeAction),    SLOT(trigger()));

    menu()->setTargetProvider(m_ui.data());


    /* Tab bar. */
    m_tabBar = new QnLayoutTabBar(this);
#ifdef Q_OS_WIN
    // tabs are drawn in the window header on windows 7 //TODO: #Elric check on windows XP
    m_tabBar->setAttribute(Qt::WA_TranslucentBackground);
#endif
    connect(m_tabBar,                       SIGNAL(closeRequested(QnWorkbenchLayout *)),    this,                                   SLOT(at_tabBar_closeRequested(QnWorkbenchLayout *)));
    connect(m_tabBar,             &QnLayoutTabBar::tabCloseRequested,     this,    &QnMainWindow::skipDoubleClick);
    connect(m_tabBar,             &QnLayoutTabBar::currentChanged,        this,    &QnMainWindow::skipDoubleClick);


    /* Tab bar layout. To snap tab bar to graphics view. */
    QVBoxLayout *tabBarLayout = new QVBoxLayout();
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);
    tabBarLayout->addStretch(0x1000);
    tabBarLayout->addWidget(m_tabBar);

    /* Layout for window buttons that can be removed from the title bar. */
    m_windowButtonsLayout = new QHBoxLayout();
    m_windowButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_windowButtonsLayout->setSpacing(2);
    m_windowButtonsLayout->addSpacing(6);
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::WhatsThisAction), false, 1.0, Qn::MainWindow_ContextHelp_Help));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::MinimizeAction)));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::EffectiveMaximizeAction), false, 1.0, Qn::MainWindow_Fullscreen_Help));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::ExitAction)));

    /* Title layout. We cannot create a widget for title bar since there appears to be
     * no way to make it transparent for non-client area windows messages. */
    m_mainMenuButton = newActionButton(action(Qn::MainMenuAction), true, 1.5, Qn::MainWindow_TitleBar_MainMenu_Help);
    connect(action(Qn::MainMenuAction), &QAction::triggered, this, &QnMainWindow::skipDoubleClick);

    m_titleLayout = new QHBoxLayout();
    m_titleLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLayout->setSpacing(2);
    m_titleLayout->addWidget(m_mainMenuButton);
    m_titleLayout->addLayout(tabBarLayout);
    m_titleLayout->addWidget(newActionButton(action(Qn::OpenNewTabAction), false, 1.0, Qn::MainWindow_TitleBar_NewLayout_Help));
    m_titleLayout->addWidget(newActionButton(action(Qn::OpenCurrentUserLayoutMenu), true));
    m_titleLayout->addStretch(0x1000);
    if (QnScreenRecorder::isSupported())
        m_titleLayout->addWidget(newActionButton(action(Qn::ToggleScreenRecordingAction), false, 1.0, Qn::MainWindow_ScreenRecording_Help));
    m_titleLayout->addWidget(newActionButton(action(Qn::OpenLoginDialogAction), false, 1.0, Qn::Login_Help));
    m_titleLayout->addLayout(m_windowButtonsLayout);

    /* Layouts. */
    m_viewLayout = new QVBoxLayout();
    m_viewLayout->setContentsMargins(0, 0, 0, 0);
    m_viewLayout->setSpacing(0);
    m_viewLayout->addWidget(m_view.data());

    m_globalLayout = new QVBoxLayout();
    m_globalLayout->setContentsMargins(0, 0, 0, 0);
    m_globalLayout->setSpacing(0);
    m_globalLayout->addLayout(m_titleLayout);
    m_globalLayout->addLayout(m_viewLayout);
    m_globalLayout->setStretchFactor(m_viewLayout, 0x1000);
    setLayout(m_globalLayout);


    /* Post-initialize. */
    updateDwmState();
#ifdef Q_OS_MACX
    setOptions(WindowButtonsVisible);
#else
    setOptions(TitleBarDraggable | WindowButtonsVisible);
#endif

    /* Open single tab. */
    action(Qn::OpenNewTabAction)->trigger();

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

    QnPtzManageDialog *manageDialog = new QnPtzManageDialog(this); //initializing instance of a singleton
    Q_UNUSED(manageDialog)
}

QnMainWindow::~QnMainWindow() {
    m_dwm = NULL;
}

QWidget *QnMainWindow::viewport() const {
    return m_view->viewport();
}

void QnMainWindow::setTitleVisible(bool visible) {
    if(m_titleVisible == visible)
        return;

    m_titleVisible = visible;
    if(visible) {
        m_globalLayout->insertLayout(0, m_titleLayout);
        setVisibleRecursively(m_titleLayout, true);
    } else {
        m_globalLayout->takeAt(0);
        m_titleLayout->setParent(NULL);
        setVisibleRecursively(m_titleLayout, false);
    }

    updateDwmState();
}

void QnMainWindow::setWindowButtonsVisible(bool visible) {
    bool currentVisibility = m_windowButtonsLayout->parent() != NULL;
    if(currentVisibility == visible)
        return;

    if(visible) {
        m_titleLayout->addLayout(m_windowButtonsLayout);
        setVisibleRecursively(m_windowButtonsLayout, true);
    } else {
        m_titleLayout->takeAt(m_titleLayout->count() - 1);
        m_windowButtonsLayout->setParent(NULL);
        setVisibleRecursively(m_windowButtonsLayout, false);
    }
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

void QnMainWindow::skipDoubleClick() {
    m_skipDoubleClick = true;
}

void QnMainWindow::minimize() {
    showMinimized();
}

void QnMainWindow::toggleTitleVisibility() {
    setTitleVisible(!isTitleVisible());
}

bool QnMainWindow::handleMessage(const QString &message) {
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    
    QnResourceList resources = QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files));
    if (resources.isEmpty())
        return false;

    menu()->trigger(Qn::DropResourcesAction, resources);
    return true;
}

QnMainWindow::Options QnMainWindow::options() const {
    return m_options;
}

void QnMainWindow::setOptions(Options options) {
    if(m_options == options)
        return;

    m_options = options;

    setWindowButtonsVisible(m_options & WindowButtonsVisible);
    m_ui->setWindowButtonsUsed(m_options & WindowButtonsVisible);
}

void QnMainWindow::updateDecorationsState() {
#ifdef Q_OS_MACX
    bool fullScreen = mac_isFullscreen((void*)winId());
#else
    bool fullScreen = isFullScreen();
#endif
    bool maximized = isMaximized();

    action(Qn::FullscreenAction)->setChecked(fullScreen);
    action(Qn::MaximizeAction)->setChecked(maximized);

#ifdef Q_OS_MACX
    bool uiTitleUsed = fullScreen;
#else
    bool uiTitleUsed = fullScreen || maximized;
#endif

    bool windowTitleUsed = !uiTitleUsed && !qnSettings->isVideoWallMode();
    setTitleVisible(windowTitleUsed);
    m_ui->setTitleUsed(uiTitleUsed && !qnSettings->isVideoWallMode());
    m_view->setLineWidth(windowTitleUsed ? 0 : 1);

    updateDwmState();
}

void QnMainWindow::updateDwmState() {
    if(isFullScreen()) {
        /* Full screen mode. */
        m_drawCustomFrame = false;
        m_frameMargins = QMargins(0, 0, 0, 0);

        if(m_dwm->isSupported() && false) { // TODO: Disable DWM for now.
            setAttribute(Qt::WA_NoSystemBackground, false);
            setAttribute(Qt::WA_TranslucentBackground, false);

            m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
            m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
            m_dwm->disableBlurBehindWindow();
        }

        /* Can't set to (0, 0, 0, 0) on Windows as in fullScreen mode context menu becomes invisible.
         * Looks like Qt bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556. */
#ifdef Q_OS_WIN
        setContentsMargins(0, 0, 0, 1);
#else
        setContentsMargins(0, 0, 0, 0);
#endif

        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_viewLayout->setContentsMargins(0, 0, 0, 0);
    } else if(m_dwm->isSupported() && m_dwm->isCompositionEnabled() && false) { // TODO: Disable DWM for now.
        /* Windowed or maximized with aero glass. */
        m_drawCustomFrame = false;
        m_frameMargins = !isMaximized() ? m_dwm->themeFrameMargins() : QMargins(0, 0, 0, 0);

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);

        m_dwm->extendFrameIntoClientArea();
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->enableBlurBehindWindow();

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(m_frameMargins.left(), 2, m_frameMargins.right(), 0);
        m_viewLayout->setContentsMargins(
            m_frameMargins.left(),
            isTitleVisible() ? 0 : m_frameMargins.top(),
            m_frameMargins.right(),
            m_frameMargins.bottom()
        );
    } else {
        /* Windowed or maximized without aero glass. */
        /*m_drawCustomFrame = true;
        m_frameMargins = !isMaximized() ? (m_dwm->isSupported() ? m_dwm->themeFrameMargins() : QMargins(8, 8, 8, 8)) : QMargins(0, 0, 0, 0);*/
#ifdef Q_OS_LINUX
        // On linux window manager cannot disable titlebar leaving border in place. Thus we have to disable decorations completely and draw our own border.
        if (isMaximized()) {
            m_drawCustomFrame = false;
            m_frameMargins = QMargins(0, 0, 0, 0);
        } else {
        m_drawCustomFrame = true;
        m_frameMargins = QMargins(2, 2, 2, 2);
        }
#else
        m_drawCustomFrame = false;
        m_frameMargins = QMargins(0, 0, 0, 0);
#endif

        if(m_dwm->isSupported() && false) { // TODO: Disable DWM for now.
            setAttribute(Qt::WA_NoSystemBackground, false);
            setAttribute(Qt::WA_TranslucentBackground, false);

            m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
            m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
            m_dwm->disableBlurBehindWindow();
        }

        setContentsMargins(0, 0, 0, 0);

        //m_titleLayout->setContentsMargins(m_frameMargins.left(), 2, m_frameMargins.right(), 0);
        m_titleLayout->setContentsMargins(2, 0, 2, 0);
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

    if(event->type() == QnEvent::WinSystemMenu) {
        if(m_mainMenuButton->isVisible())
            m_mainMenuButton->click();
            
        QApplication::sendEvent(m_ui.data(), event);
        result = true;
    }

    if(m_dwm != NULL)
        result |= m_dwm->widgetEvent(event);

    return result;
}

void QnMainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    menu()->trigger(Qn::ExitAction);
}

void QnMainWindow::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    if(event->button() == Qt::RightButton && windowFrameSectionAt(event->pos()) == Qt::TitleBarArea) {
        QContextMenuEvent e(QContextMenuEvent::Mouse, m_tabBar->mapFrom(this, event->pos()), event->globalPos(), event->modifiers());
        QApplication::sendEvent(m_tabBar, &e);
        event->accept();
    }

    m_skipDoubleClick = false;
}

void QnMainWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    base_type::mouseDoubleClickEvent(event);

#ifndef Q_OS_MACX
    if(event->button() == Qt::LeftButton && windowFrameSectionAt(event->pos()) == Qt::TitleBarArea) {
        QPoint tabBarPos = m_tabBar->mapFrom(this, event->pos());
        if (m_tabBar->tabAt(tabBarPos) >= 0) {
            m_skipDoubleClick = false;
            return;
        }

        if (!m_skipDoubleClick)
            action(Qn::EffectiveMaximizeAction)->toggle();
        event->accept();
    }
    m_skipDoubleClick = false;
#endif
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

void QnMainWindow::dragEnterEvent(QDragEnterEvent *event) {
    QnResourceList resources = QnWorkbenchResource::deserializeResources(event->mimeData());

    QnResourceList media;   // = resources.filtered<QnMediaResource>();
    QnResourceList layouts; // = resources.filtered<QnLayoutResource>();
    QnResourceList servers; // = resources.filtered<QnMediaServerResource>();

    foreach( QnResourcePtr res, resources )
    {
        if( dynamic_cast<QnMediaResource*>(res.data()) )
            media.push_back( res );
        if( res.dynamicCast<QnLayoutResource>() )
            layouts.push_back( res );
        if( res.dynamicCast<QnMediaServerResource>() )
            servers.push_back( res );
    }

    m_dropResources = media;
    m_dropResources << layouts;
    m_dropResources << servers;

    if (m_dropResources.empty())
        return;

    event->acceptProposedAction();
}

void QnMainWindow::dragMoveEvent(QDragMoveEvent *event) {
    if(m_dropResources.empty())
        return;

    event->acceptProposedAction();
}

void QnMainWindow::dragLeaveEvent(QDragLeaveEvent *) {
    m_dropResources = QnResourceList();
}

void QnMainWindow::dropEvent(QDropEvent *event) {
    menu()->trigger(Qn::DropResourcesIntoNewLayoutAction, m_dropResources);

    event->acceptProposedAction();
}

void QnMainWindow::keyPressEvent(QKeyEvent *event) {
    base_type::keyPressEvent(event);

    if (!action(Qn::ToggleTourModeAction)->isChecked())
        return;

    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_Control)
        return;
    menu()->trigger(Qn::ToggleTourModeAction);
}

void QnMainWindow::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);
}

bool QnMainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
    /* Note that we may get here from destructor, so check for dwm is needed. */
    if(m_dwm && m_dwm->widgetNativeEvent(eventType, message, result))
        return true;

    return base_type::nativeEvent(eventType, message, result);
}

Qt::WindowFrameSection QnMainWindow::windowFrameSectionAt(const QPoint &pos) const {
    if(isFullScreen())
        return Qt::NoSection;

    Qt::WindowFrameSection result = Qn::toNaturalQtFrameSection(Qn::calculateRectangularFrameSections(rect(), QnGeometry::eroded(rect(), m_frameMargins), QRect(pos, pos)));

    if((m_options & TitleBarDraggable) && result == Qt::NoSection && pos.y() <= m_tabBar->mapTo(const_cast<QnMainWindow *>(this), m_tabBar->rect().bottomRight()).y())
        result = Qt::TitleBarArea;

    return result;
}

void QnMainWindow::at_fileOpenSignalizer_activated(QObject *, QEvent *event) {
    if(event->type() != QEvent::FileOpen) {
        qnWarning("Expected event of type %1, received an event of type %2.", static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    handleMessage(static_cast<QFileOpenEvent *>(event)->file());
}

void QnMainWindow::at_tabBar_closeRequested(QnWorkbenchLayout *layout) {
    QnWorkbenchLayoutList layouts;
    layouts.push_back(layout);
    menu()->trigger(Qn::CloseLayoutAction, layouts);
}
