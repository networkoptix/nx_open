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
#include <utils/common/event_processors.h>
#include <utils/common/environment.h>

#include <core/resourcemanagment/resource_discovery_manager.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_pool_user_watcher.h>

#include <api/app_server_connection.h>
#include <api/session_manager.h>

#include "ui/actions/action_manager.h"

#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/view/gradient_background_painter.h"

#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_ui.h"
#include "ui/workbench/workbench_synchronizer.h"
#include "ui/workbench/workbench_action_handler.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench_resource.h"

#include "ui/style/skin.h"
#include "ui/style/globals.h"
#include "ui/style/proxy_style.h"

#include "ui/events/system_menu_event.h"

#include "file_processor.h"
#include "utils/settings.h"

#include "resource_tree_widget.h"
#include "dwm.h"
#include "layout_tab_bar.h"

namespace {

    QToolButton *newActionButton(QAction *action, qreal sizeMultiplier = 1.0) {
        QToolButton *button = new QToolButton();
        button->setDefaultAction(action);

        qreal aspectRatio = QnGeometry::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));
        int iconHeight = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, 0, button) * sizeMultiplier;
        int iconWidth = iconHeight * aspectRatio;
        button->setFixedSize(iconWidth, iconHeight);

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


QnMainWindow::QnMainWindow(QnWorkbenchContext *context, QWidget *parent, Qt::WindowFlags flags): 
    QWidget(parent, flags | Qt::CustomizeWindowHint),
    QnWorkbenchContextAware(context),
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

    /* Set up properties. */
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

    m_backgroundPainter.reset(new QnGradientBackgroundPainter(120.0));
    m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);


    /* Set up model & control machinery. */
    display()->setStreamsSynchronized(true, DATETIME_NOW, 1.0);
    display()->setScene(scene);
    display()->setView(m_view);
    display()->setNormalMarginFlags(Qn::MarginsAffectSize | Qn::MarginsAffectPosition);

    m_controller = new QnWorkbenchController(this);
    m_ui = new QnWorkbenchUi(this);
    m_ui->setFlags(QnWorkbenchUi::HideWhenZoomed | QnWorkbenchUi::AdjustMargins);

    m_actionHandler = new QnWorkbenchActionHandler(this);
    m_actionHandler->setWidget(this);

    /* Set up actions. */
    addAction(action(Qn::NextLayoutAction));
    addAction(action(Qn::PreviousLayoutAction));
    addAction(action(Qn::SaveCurrentLayoutAction));
    addAction(action(Qn::SaveCurrentLayoutAsAction));
    addAction(action(Qn::ExitAction));
    addAction(action(Qn::FullscreenAction));
    addAction(action(Qn::AboutAction));
    addAction(action(Qn::SystemSettingsAction));
    addAction(action(Qn::OpenFileAction));
    addAction(action(Qn::ConnectToServerAction));
    addAction(action(Qn::OpenNewTabAction));
    addAction(action(Qn::OpenNewWindowAction));
    addAction(action(Qn::CloseLayoutAction));
    addAction(action(Qn::MainMenuAction));
    addAction(action(Qn::YouTubeUploadAction));
    addAction(action(Qn::EditTagsAction));
    addAction(action(Qn::OpenInFolderAction));
    addAction(action(Qn::RemoveLayoutItemAction));
    addAction(action(Qn::RemoveFromServerAction));
    addAction(action(Qn::SelectAllAction));
    addAction(action(Qn::TakeScreenshotAction));
    addAction(action(Qn::IncrementDebugCounterAction));
    addAction(action(Qn::DecrementDebugCounterAction));

    connect(action(Qn::FullscreenAction),   SIGNAL(toggled(bool)),                          this,                                   SLOT(setFullScreen(bool)));
    connect(action(Qn::MinimizeAction),     SIGNAL(triggered()),                            this,                                   SLOT(minimize()));
    connect(action(Qn::MainMenuAction),     SIGNAL(triggered()),                            this,                                   SLOT(at_mainMenuAction_triggered()));

    menu()->setTargetProvider(m_ui);


    /* Tab bar. */
    m_tabBar = new QnLayoutTabBar(this);
    m_tabBar->setAttribute(Qt::WA_TranslucentBackground);

    connect(m_tabBar,                       SIGNAL(closeRequested(QnWorkbenchLayout *)),    this,                                   SLOT(at_tabBar_closeRequested(QnWorkbenchLayout *)));


    /* Tab bar layout. To snap tab bar to graphics view. */
    QVBoxLayout *tabBarLayout = new QVBoxLayout();
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);
    tabBarLayout->addStretch(0x1000);
    tabBarLayout->addWidget(m_tabBar);

    /* We want the main menu button to activate the corresponding action so that
     * main menu is updated. However, menu buttons do not activate their corresponding 
     * actions as they do not receive release events. We work this around by making
     * some hacky connections. */
    m_mainMenuButton = newActionButton(action(Qn::DarkMainMenuAction), 1.5);
    m_mainMenuButton->setPopupMode(QToolButton::InstantPopup);

    disconnect(m_mainMenuButton,            SIGNAL(pressed()),                              m_mainMenuButton,                       SLOT(_q_buttonPressed()));
    connect(m_mainMenuButton,               SIGNAL(pressed()),                              m_mainMenuButton->defaultAction(),      SLOT(trigger()));
    connect(m_mainMenuButton,               SIGNAL(pressed()),                              m_mainMenuButton,                       SLOT(_q_buttonPressed()));

    /* Layout for window buttons that can be removed from the title bar. */
    m_windowButtonsLayout = new QHBoxLayout();
    m_windowButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_windowButtonsLayout->setSpacing(2);
    m_windowButtonsLayout->addSpacing(6);
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::MinimizeAction)));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::FullscreenAction)));
    m_windowButtonsLayout->addWidget(newActionButton(action(Qn::ExitAction)));

    /* Title layout. We cannot create a widget for title bar since there appears to be
     * no way to make it transparent for non-client area windows messages. */
    m_titleLayout = new QHBoxLayout();
    m_titleLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLayout->setSpacing(2);
    m_titleLayout->addWidget(m_mainMenuButton);
    m_titleLayout->addLayout(tabBarLayout);
    m_titleLayout->addWidget(newActionButton(action(Qn::OpenNewTabAction)));
    m_titleLayout->addStretch(0x1000);
    m_titleLayout->addWidget(newActionButton(action(Qn::ConnectToServerAction)));
    m_titleLayout->addLayout(m_windowButtonsLayout);

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

    /* Post-initialize. */
    updateDwmState();
    setOptions(TitleBarDraggable | WindowButtonsVisible);

    /* Open single tab. */
    action(Qn::OpenNewTabAction)->trigger();


    connect(action(Qn::IncrementDebugCounterAction), SIGNAL(triggered()), this, SLOT(test()));
}

QnMainWindow::~QnMainWindow() {
    m_dwm = NULL;
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

void QnMainWindow::setFullScreen(bool fullScreen) {
    if(fullScreen == isFullScreen())
        return;

    if(fullScreen) {
        showFullScreen();
    } else {
        showNormal();
    }
}

void QnMainWindow::toggleFullScreen() {
    setFullScreen(!isFullScreen());
}

void QnMainWindow::minimize() {
    setWindowState(Qt::WindowMinimized | windowState());
}

void QnMainWindow::toggleTitleVisibility() {
    setTitleVisible(!isTitleVisible());
}

void QnMainWindow::handleMessage(const QString &message) {
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    
    menu()->trigger(Qn::DropResourcesAction, QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files)));
}

QnMainWindow::Options QnMainWindow::options() const {
    return m_options;
}

void QnMainWindow::setOptions(Options options) {
    if(m_options == options)
        return;

    m_options = options;

    updateTitleBarDraggable();
    setWindowButtonsVisible(m_options & WindowButtonsVisible);
    m_ui->setWindowButtonsUsed(m_options & WindowButtonsVisible);
}

void QnMainWindow::updateFullScreenState() {
    bool fullScreen = isFullScreen();

    action(Qn::FullscreenAction)->setChecked(fullScreen);

    setTitleVisible(!fullScreen);
    m_ui->setTitleUsed(fullScreen);
    m_view->setLineWidth(isFullScreen() ? 0 : 1);

    updateDwmState();
}

void QnMainWindow::updateDwmState() {
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

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000); /* So that the window is click-draggable no matter where the user clicked. */

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left(), 2, frameMargins.right(), 0);
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

        m_dwm->enableFrameEmulation();
        m_dwm->setEmulatedFrameMargins(frameMargins);
        m_dwm->setEmulatedTitleBarHeight(0x1000);

        setContentsMargins(0, 0, 0, 0);

        m_titleLayout->setContentsMargins(frameMargins.left(), 2, frameMargins.right(), 0);
        m_viewLayout->setContentsMargins(
            frameMargins.left(),
            isTitleVisible() ? 0 : frameMargins.top(),
            frameMargins.right(),
            frameMargins.bottom()
        );
    }

    updateTitleBarDraggable();
}

void QnMainWindow::updateTitleBarDraggable() {
    if(isFullScreen()) {
        m_dwm->disableTitleBarDrag();
    } else {
        m_dwm->enableTitleBarDrag(m_options & TitleBarDraggable);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnMainWindow::event(QEvent *event) {
    bool result = base_type::event(event);

    switch(event->type()) {
    case QnSystemMenuEvent::SystemMenu:
        menu()->trigger(Qn::MainMenuAction);
        return true;
    case QEvent::NonClientAreaMouseButtonRelease:
        ncMouseReleaseEvent(static_cast<QMouseEvent *>(event));
        break;
    case QEvent::NonClientAreaMouseButtonDblClick:
        ncMouseDoubleClickEvent(static_cast<QMouseEvent *>(event));
        break;
    default:
        break;
    }

    if(m_dwm != NULL)
        result |= m_dwm->widgetEvent(event);

    return result;
}

void QnMainWindow::ncMouseReleaseEvent(QMouseEvent *event) {
    if(event->button() != Qt::RightButton)
        return;

    if(event->pos().y() > m_tabBar->mapTo(this, m_tabBar->rect().bottomRight()).y())
        return;

    QContextMenuEvent e(QContextMenuEvent::Mouse, m_tabBar->mapFrom(this, event->pos()), event->globalPos(), event->modifiers());
    QApplication::sendEvent(m_tabBar, &e);
}

void QnMainWindow::ncMouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return;

    if(event->pos().y() > m_tabBar->mapTo(this, m_tabBar->rect().bottomRight()).y())
        return;

    toggleFullScreen();
}

void QnMainWindow::changeEvent(QEvent *event) {
    if(event->type() == QEvent::WindowStateChange)
        updateFullScreenState();

    base_type::changeEvent(event);
}

void QnMainWindow::paintEvent(QPaintEvent *event) {
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

void QnMainWindow::dragEnterEvent(QDragEnterEvent *event) {
    m_dropResources = QnWorkbenchResource::deserializeResources(event->mimeData());
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

#ifdef Q_OS_WIN
bool QnMainWindow::winEvent(MSG *message, long *result)
{
    if(m_dwm->widgetWinEvent(message, result))
        return true;

    return base_type::winEvent(message, result);
}
#endif

void QnMainWindow::at_fileOpenSignalizer_activated(QObject *, QEvent *event) {
    if(event->type() != QEvent::FileOpen) {
        qnWarning("Expected event of type %1, received an event of type %2.", static_cast<int>(QEvent::FileOpen), static_cast<int>(event->type()));
        return;
    }

    handleMessage(static_cast<QFileOpenEvent *>(event)->file());
}

void QnMainWindow::at_sessionManager_error(int error) {
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

void QnMainWindow::at_tabBar_closeRequested(QnWorkbenchLayout *layout) {
    QnWorkbenchLayoutList layouts;
    layouts.push_back(layout);
    menu()->trigger(Qn::CloseLayoutAction, layouts);
}

void QnMainWindow::at_mainMenuAction_triggered() {
    if(!m_mainMenuButton->isVisible())
        return;

    const char *inShowMenuPropertyName = "_qn_inShowMenu";

    QMenu *menu = m_mainMenuButton->defaultAction()->menu();
    if(m_mainMenuButton->property(inShowMenuPropertyName).toBool()) {
        if(menu)
            menu->hide();
    } else {
        m_mainMenuButton->setProperty(inShowMenuPropertyName, true);
        m_mainMenuButton->click(); /* This call starts event loop. */
        m_mainMenuButton->setProperty(inShowMenuPropertyName, false);
    }
}
