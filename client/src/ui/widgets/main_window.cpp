#include "main_window.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFileDialog>
#include <QToolButton>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
#include <QNetworkReply>
#include <QFileOpenEvent>

#include <utils/common/warnings.h>
#include <utils/common/event_signalizer.h>
#include <utils/common/environment.h>

#include <core/resourcemanagment/resource_discovery_manager.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_pool_user_watcher.h>

#include <api/AppServerConnection.h>
#include <api/SessionManager.h>

#include "ui/actions/action_manager.h"

#include "ui/dialogs/aboutdialog.h"
#include "ui/dialogs/logindialog.h"
#include "ui/dialogs/camerasettingsdialog.h"
#include "ui/dialogs/multiplecamerasettingsdialog.h"
#include "ui/dialogs/serversettingsdialog.h"
#include "ui/dialogs/tagseditdialog.h"
#include "ui/dialogs/layout_save_dialog.h"
#include "ui/preferences/preferencesdialog.h"
#include "youtube/youtubeuploaddialog.h"

#include "ui/mixins/sync_play_mixin.h"
#include "ui/mixins/render_watch_mixin.h"

#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/view/blue_background_painter.h"

#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_ui.h"
#include "ui/workbench/workbench_synchronizer.h"
#include "ui/workbench/workbench_action_handler.h"

#include "ui/widgets/resource_tree_widget.h"

#include "ui/style/skin.h"
#include "ui/style/globals.h"
#include "ui/style/proxy_style.h"

#include "file_processor.h"
#include "settings.h"

#include "dwm.h"
#include "layout_tab_bar.h"
#include "system_menu_event.h"

#include "eventmanager.h"

Q_DECLARE_METATYPE(QnWorkbenchLayout *);

namespace {

    QToolButton *newActionButton(QAction *action)
    {
        QToolButton *button = new QToolButton();
        button->setDefaultAction(action);

        qreal aspectRatio = SceneUtility::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));
        int iconHeight = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, 0, button);
        int iconWidth = iconHeight * aspectRatio;
        button->setIconSize(QSize(iconWidth, iconHeight));

        /* Tool buttons may end up having wrong aspect ratio. We don't allow that. */
        QSize sizeHint = button->sizeHint();
        int buttonHeight = sizeHint.height();
        int buttonWidth = buttonHeight * aspectRatio;
        button->setFixedSize(buttonWidth, buttonHeight);

        return button;
    }

    void setVisibleRecursively(QLayout *layout, bool visible)
    {
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


QnMainWindow::QnMainWindow(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags | Qt::CustomizeWindowHint),
      m_controller(0),
      m_drawCustomFrame(false),
      m_titleVisible(true),
      m_dwm(NULL)
{
    /* We want to receive system menu event on Windows. */
    QnSystemMenuEvent::initialize();

    /* And file open events on Mac. */
    QnSingleEventSignalizer *fileOpenSignalizer = new QnSingleEventSignalizer(this);
    fileOpenSignalizer->setEventType(QEvent::FileOpen);
    qApp->installEventFilter(fileOpenSignalizer);
    connect(fileOpenSignalizer,             SIGNAL(activated(QObject *, QEvent *)),         this,                                   SLOT(at_fileOpenSignalizer_activated(QObject *, QEvent *)));

    /* Set up dwm. */
    m_dwm = new QnDwm(this);

    connect(m_dwm,                          SIGNAL(compositionChanged(bool)),               this,                                   SLOT(updateDwmState()));
    connect(m_dwm,                          SIGNAL(titleBarDoubleClicked()),                this,                                   SLOT(toggleFullScreen()));

    /* Set up QWidget. */
    setWindowTitle(QApplication::applicationName());
    setAcceptDrops(true);
    setMinimumWidth(minimalWindowWidth);
    setMinimumHeight(minimalWindowHeight);
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::black);
        setPalette(palette);
    }


    /* Set up scene & view. */
    QGraphicsScene *scene = new QGraphicsScene(this);
    m_view = new QnGraphicsView(scene);
    m_view->setPaintFlags(QnGraphicsView::BACKGROUND_DONT_INVOKE_BASE | QnGraphicsView::FOREGROUND_DONT_INVOKE_BASE);
    m_view->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_view->setLineWidth(1);
    m_view->setAutoFillBackground(true);
    {
        /* Adjust palette so that inherited background painting is not needed. */
        QPalette palette = m_view->palette();
        palette.setColor(QPalette::Background, Qt::black);
        palette.setColor(QPalette::Base, Qt::black);
        m_view->setPalette(palette);
    }

    m_backgroundPainter.reset(new QnBlueBackgroundPainter(120.0));
    m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);


    /* Set up model & control machinery. */
    const QSizeF defaultCellSize = QSizeF(16000.0, 12000.0); /* Graphics scene has problems with handling mouse events on small scales, so the larger these numbers, the better. */
    const QSizeF defaultSpacing = QSizeF(2500.0, 2500.0);
    m_workbench = new QnWorkbench(this);
    m_workbench->mapper()->setCellSize(defaultCellSize);
    m_workbench->mapper()->setSpacing(defaultSpacing);

    m_display = new QnWorkbenchDisplay(m_workbench, this);
	m_display->initSyncPlay();
    m_display->setScene(scene);
    m_display->setView(m_view);
    m_display->setMarginFlags(Qn::MARGINS_AFFECT_POSITION);

    m_controller = new QnWorkbenchController(m_display, this);
    m_ui = new QnWorkbenchUi(m_display, this);
    m_ui->setFlags(QnWorkbenchUi::HIDE_WHEN_ZOOMED | QnWorkbenchUi::AFFECT_MARGINS_WHEN_NORMAL);

    m_synchronizer = new QnWorkbenchSynchronizer(this);
    m_synchronizer->setWorkbench(m_workbench);

    m_userWatcher = new QnResourcePoolUserWatcher(qnResPool, this);

    m_actionHandler = new QnWorkbenchActionHandler(this);
    m_actionHandler->setWorkbench(m_workbench);
    m_actionHandler->setSynchronizer(m_synchronizer);
    m_actionHandler->setWidget(this);

    connect(m_userWatcher,                  SIGNAL(userChanged(const QnUserResourcePtr &)), m_synchronizer,                         SLOT(setUser(const QnUserResourcePtr &)));
    connect(qnSettings,                     SIGNAL(lastUsedConnectionChanged()),            this,                                   SLOT(at_settings_lastUsedConnectionChanged()));
    connect(m_synchronizer,                 SIGNAL(started()),                              qnAction(Qn::OpenSingleLayoutAction),   SLOT(trigger()));

    /* Set up actions. */
    addAction(qnAction(Qn::ExitAction));
    addAction(qnAction(Qn::FullscreenAction));
    addAction(qnAction(Qn::AboutAction));
    addAction(qnAction(Qn::SystemSettingsAction));
    addAction(qnAction(Qn::OpenFileAction));
    addAction(qnAction(Qn::ConnectionSettingsAction));
    addAction(qnAction(Qn::NewLayoutAction));
    addAction(qnAction(Qn::CloseLayoutAction));
    addAction(qnAction(Qn::MainMenuAction));
    addAction(qnAction(Qn::YouTubeUploadAction));
    addAction(qnAction(Qn::EditTagsAction));
    addAction(qnAction(Qn::OpenInFolderAction));
    addAction(qnAction(Qn::RemoveLayoutItemAction));
    addAction(qnAction(Qn::RemoveFromServerAction));

    connect(qnAction(Qn::ExitAction),       SIGNAL(triggered()),                            this,                                   SLOT(close()));
    connect(qnAction(Qn::FullscreenAction), SIGNAL(toggled(bool)),                          this,                                   SLOT(setFullScreen(bool)));

    qnMenu->setTargetProvider(m_ui);

    connect(SessionManager::instance(),     SIGNAL(error(int)),                             this,                                   SLOT(at_sessionManager_error(int)));


    /* Tab bar. */
    m_tabBar = new QnLayoutTabBar(this);
    m_tabBar->setAttribute(Qt::WA_TranslucentBackground);
    m_tabBar->setWorkbench(m_workbench);

    connect(m_tabBar,                       SIGNAL(closeRequested(QnWorkbenchLayout *)),    this,                                   SLOT(at_tabBar_closeRequested(QnWorkbenchLayout *)));


    /* Tab bar layout. To snap tab bar to graphics view. */
    QVBoxLayout *tabBarLayout = new QVBoxLayout();
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);
    tabBarLayout->addStretch(0x1000);
    tabBarLayout->addWidget(m_tabBar);

    /* Title layout. We cannot create a widget for title bar since there appears to be
     * no way to make it transparent for non-client area windows messages. */
    m_mainMenuButton = newActionButton(qnAction(Qn::MainMenuAction));
    m_mainMenuButton->setPopupMode(QToolButton::InstantPopup);

    m_mainMenuButton->setIcon(Skin::icon(QLatin1String("logo_icon2_dark.png")));
    m_titleLayout = new QHBoxLayout();
    m_titleLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLayout->setSpacing(0);
    m_titleLayout->addWidget(m_mainMenuButton);
    m_titleLayout->addLayout(tabBarLayout);
    m_titleLayout->addWidget(newActionButton(qnAction(Qn::NewLayoutAction)));
    m_titleLayout->addStretch(0x1000);
    m_titleLayout->addWidget(newActionButton(qnAction(Qn::ConnectionSettingsAction)));
    m_titleLayout->addWidget(newActionButton(qnAction(Qn::SystemSettingsAction)));
    m_titleLayout->addWidget(newActionButton(qnAction(Qn::FullscreenAction)));
    m_titleLayout->addWidget(newActionButton(qnAction(Qn::ExitAction)));

    /* Layouts. */
    m_viewLayout = new QVBoxLayout();
    m_viewLayout->setContentsMargins(0, 0, 0, 0);
    m_viewLayout->setSpacing(0);
    m_viewLayout->addWidget(m_view);

    m_globalLayout = new QVBoxLayout();
    m_globalLayout->setContentsMargins(0, 0, 0, 0);
    m_globalLayout->setSpacing(0);
    m_globalLayout->addLayout(m_titleLayout);
    m_globalLayout->addLayout(m_viewLayout);
    m_globalLayout->setStretchFactor(m_viewLayout, 0x1000);
    setLayout(m_globalLayout);

    /* Process input files. */
    QList<QString> files;
    for (int i = 1; i < argc; ++i)
        files.push_back(QFile::decodeName(argv[i]));
    qnMenu->trigger(Qn::ResourceDropAction, QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files)));

    /* Update state. */
    updateDwmState();
    at_settings_lastUsedConnectionChanged();
}

QnMainWindow::~QnMainWindow()
{
    return;
}

void QnMainWindow::setTitleVisible(bool visible) 
{
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

void QnMainWindow::setFullScreen(bool fullScreen) 
{
    if(fullScreen == isFullScreen())
        return;

    if(fullScreen) {
        showFullScreen();
    } else {
        showNormal();
    }
}

void QnMainWindow::toggleFullScreen() 
{
    setFullScreen(!isFullScreen());
}

void QnMainWindow::toggleTitleVisibility() 
{
    setTitleVisible(!isTitleVisible());
}

void QnMainWindow::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    
    qnMenu->trigger(Qn::ResourceDropAction, QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files)));
}

void QnMainWindow::updateFullScreenState() 
{
    bool fullScreen = isFullScreen();

    qnAction(Qn::FullscreenAction)->setChecked(fullScreen);

    setTitleVisible(!fullScreen);
    m_ui->setTitleUsed(fullScreen);
    m_view->setLineWidth(isFullScreen() ? 0 : 1);

    updateDwmState();
}

void QnMainWindow::updateDwmState()
{
    if(!m_dwm->isSupported()) {
        m_drawCustomFrame = false;

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_viewLayout->setContentsMargins(0, 0, 0, 0);

        return; /* Do nothing on systems where our tricks are not supported. */
    }

    if(isFullScreen()) {
        /* Full screen mode. */

        m_drawCustomFrame = false;

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_dwm->disableSystemWindowPainting();
        m_dwm->disableTransparentErasing();
        m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->disableBlurBehindWindow();
        m_dwm->enableDoubleClickProcessing();
        m_dwm->disableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->setEmulatedTitleBarHeight(0x1000); /* So that the window is click-draggable no matter where the user clicked. */

        /* Can't set to (0, 0, 0, 0) on Windows as in fullScreen mode context menu becomes invisible.
         * Looks like Qt bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556. */
#ifdef Q_OS_WIN
        setContentsMargins(0, 0, 0, 1);
#else
        setContentsMargins(0, 0, 0, 0);
#endif

        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_viewLayout->setContentsMargins(0, 0, 0, 0);
    } else if(m_dwm->isCompositionEnabled()) {
        /* Windowed with aero glass. */

        m_drawCustomFrame = false;

        if(!m_dwm->isCompositionEnabled())
            qnWarning("Transitioning to glass state when aero composition is disabled. Expect display artifacts.");

        QMargins frameMargins = m_dwm->themeFrameMargins();

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);

        m_dwm->disableSystemWindowPainting();
        m_dwm->enableTransparentErasing();
        m_dwm->extendFrameIntoClientArea();
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->enableBlurBehindWindow(); /* For reasons unknown, this call is needed to prevent display artifacts. */
        m_dwm->enableDoubleClickProcessing();
        m_dwm->enableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000); /* So that the window is click-draggable no matter where the user clicked. */

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left(), 2, 2, 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left(),
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right(),
            frameMargins.bottom()
        );
    } else {
        /* Windowed without aero glass. */

        m_drawCustomFrame = true;

        QMargins frameMargins = m_dwm->themeFrameMargins();

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAttribute(Qt::WA_TranslucentBackground, false);

        m_dwm->disableSystemWindowPainting();
        m_dwm->disableTransparentErasing();
        m_dwm->extendFrameIntoClientArea(QMargins(0, 0, 0, 0));
        m_dwm->setCurrentFrameMargins(QMargins(0, 0, 0, 0));
        m_dwm->disableBlurBehindWindow();
        m_dwm->enableDoubleClickProcessing();
        m_dwm->enableTitleBarDrag();

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000);

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left(), 2, 2, 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left(),
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right(),
            frameMargins.bottom()
        );
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnMainWindow::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QnSystemMenuEvent::SystemMenu) {
        qnAction(Qn::MainMenuAction)->trigger();
        return true;
    }

    if(m_dwm != NULL)
        result |= m_dwm->widgetEvent(event);

    return result;
}

void QnMainWindow::changeEvent(QEvent *event) 
{
    if(event->type() == QEvent::WindowStateChange)
        updateFullScreenState();

    base_type::changeEvent(event);
}

void QnMainWindow::paintEvent(QPaintEvent *event) 
{
    base_type::paintEvent(event);

    if(m_drawCustomFrame) {
        QPainter painter(this);

        painter.setPen(QPen(qnGlobals->frameColor(), 3));
        painter.drawRect(QRect(
            0,
            0,
            width() - 1,
            height() - 1
        ));
    }
}

void QnMainWindow::resizeEvent(QResizeEvent *event) {
    QRect rect = this->rect();
    QRect viewGeometry = m_view->geometry();

    m_dwm->setNonErasableContentMargins(QMargins(
        viewGeometry.left(),
        viewGeometry.top(),
        rect.right() - viewGeometry.right(),
        rect.bottom() - viewGeometry.bottom()
    ));

    base_type::resizeEvent(event);
}

#ifdef Q_OS_WIN
bool QnMainWindow::winEvent(MSG *message, long *result)
{
    if(m_dwm->widgetWinEvent(message, result))
        return true;

    return base_type::winEvent(message, result);
}
#endif

void QnMainWindow::at_fileOpenSignalizer_activated(QObject *, QEvent *event)
{
    if(event->type() != QEvent::FileOpen) {
        qnWarning("Expected event of type %1, received an event of type %2.", static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    handleMessage(static_cast<QFileOpenEvent *>(event)->file());
}

void QnMainWindow::at_sessionManager_error(int error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError: // ### remove ?
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::UnknownNetworkError:
        // Do not show popup box! It's annoying!
        // Show something like color label in the main window.
        // showAuthenticationDialog();
        break;

    default:
        break;
    }
}

void QnMainWindow::at_settings_lastUsedConnectionChanged() {
    m_userWatcher->setUserName(qnSettings->lastUsedConnection().url.userName());
}

void QnMainWindow::at_tabBar_closeRequested(QnWorkbenchLayout *layout) {
    QnWorkbenchLayoutList layouts;
    layouts.push_back(layout);
    qnMenu->trigger(Qn::CloseLayoutAction, layouts);
}

