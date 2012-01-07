#include "mainwnd.h"

#include <QtCore/QFile>

#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtGui/QToolBar>
#include <QtGui/QToolButton>

#include <QtNetwork/QNetworkReply>

#include <api/AppServerConnection.h>
#include <api/SessionManager.h>

#include <core/resourcemanagment/asynch_seacher.h>
#include <core/resourcemanagment/resource_pool.h>

#if 0
#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/video_cam_layout/start_screen_content.h"
#endif

#include "ui/context_menu_helper.h"

#include "ui/dialogs/logindialog.h"
#include "ui/preferences/preferences_wnd.h"

#include "ui/mixins/sync_play_mixin.h"
#include "ui/mixins/render_watch_mixin.h"

#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/view/blue_background_painter.h"

#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/workbench/workbench_display.h"

#include "ui/widgets3/tabwidget.h"

#include "ui/skin/skin.h"

#include <utils/common/warnings.h>

#include "file_processor.h"
#include "settings.h"

MainWnd *MainWnd::s_instance = 0;

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags),
      m_controller(0)
{
    if (s_instance)
        qnWarning("Several instances of main window created, expect problems.");
    else
        s_instance = this;

    /* Set up QWidget. */
    setWindowTitle(QApplication::applicationName());

    setAttribute(Qt::WA_QuitOnClose);
    setAcceptDrops(true);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), app_bkr_color);
    setPalette(pal);

    const int min_width = 800;
    setMinimumWidth(min_width);
    setMinimumHeight(min_width * 3 / 4);

    /* Set up scene & view. */
    QGraphicsScene *scene = new QGraphicsScene(this);
    m_view = new QnGraphicsView(scene, this);

    m_backgroundPainter.reset(new QnBlueBackgroundPainter(120.0));
    m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);

    /* Set up model & control machinery. */
    const QSizeF defaultCellSize = QSizeF(150.0, 100.0);
    const QSizeF defaultSpacing = QSizeF(25.0, 25.0);
    m_workbench = new QnWorkbench(this);
    m_workbench->mapper()->setCellSize(defaultCellSize);
    m_workbench->mapper()->setSpacing(defaultSpacing);

    m_display = new QnWorkbenchDisplay(m_workbench, this);
    m_display->setScene(scene);
    m_display->setView(m_view);

    m_controller = new QnWorkbenchController(m_display, this);

    QnRenderWatchMixin *renderWatcher = new QnRenderWatchMixin(m_display, this);
    new QnSyncPlayMixin(m_display, renderWatcher, this);
    connect(renderWatcher, SIGNAL(displayingStateChanged(QnAbstractRenderer *, bool)), m_display, SLOT(onDisplayingStateChanged(QnAbstractRenderer *, bool)));


    /* Process input files. */
    for (int i = 1; i < argc; ++i)
        m_controller->drop(fromNativePath(QFile::decodeName(argv[i])), QPointF(0, 0));

    /* Prepare UI. */
    m_tabWidget = new TabWidget(this);
    m_tabWidget->setMovable(true);
    m_tabWidget->setTabsClosable(true);
    static_cast<TabWidget *>(m_tabWidget)->setSelectionBehaviorOnRemove(TabWidget::SelectPreviousTab);

    connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
    connect(m_tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    QToolButton *newTabButton = new QToolButton(m_tabWidget);
    newTabButton->setToolTip(tr("New Tab"));
    newTabButton->setShortcut(QKeySequence::New);
    newTabButton->setIcon(Skin::icon(QLatin1String("plus.png")));
    newTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(newTabButton, SIGNAL(clicked()), this, SLOT(addTab()));
    m_tabWidget->setCornerWidget(newTabButton, Qt::TopLeftCorner);

    setCentralWidget(m_tabWidget);

    // Can't set 0,0,0,0 on Windows as in fullScreen mode context menu becomes invisible
    // Looks like Qt bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556
#ifdef Q_OS_WIN
    setContentsMargins(0, 1, 0, 0);
#else
    setContentsMargins(0, 0, 0, 0);
#endif


    // toolbars
    QToolBar *toolBar = new QToolBar(this);
    toolBar->setAllowedAreas(Qt::TopToolBarArea);
    toolBar->addAction(&cm_exit);
    toolBar->addAction(&cm_toggle_fullscreen);
    toolBar->addAction(&cm_preferences);
    toolBar->addAction(&cm_showNavTree);
    addToolBar(Qt::TopToolBarArea, toolBar);


    // actions
    connect(&cm_exit, SIGNAL(triggered()), this, SLOT(close()));
    addAction(&cm_exit);

    connect(&cm_toggle_fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
    addAction(&cm_toggle_fullscreen);

    connect(&cm_preferences, SIGNAL(triggered()), this, SLOT(editPreferences()));
    addAction(&cm_preferences);

    addAction(&cm_showNavTree);

    QAction *reconnectAction = new QAction(Skin::icon(QLatin1String("connect.png")), tr("Reconnect"), this);
    connect(reconnectAction, SIGNAL(triggered()), this, SLOT(appServerAuthenticationRequired()));
    toolBar->addAction(reconnectAction);

    connect(SessionManager::instance(), SIGNAL(error(int)), this, SLOT(appServerError(int)));

    addTab();

    showFullScreen();
}

MainWnd::~MainWnd()
{
    s_instance = 0;
}

Q_DECLARE_METATYPE(QnWorkbenchLayout *) // ###

void MainWnd::addTab()
{
    QWidget *widget = new QWidget(m_tabWidget);
    QVBoxLayout *layout = new QVBoxLayout(widget); // ensure widget's layout
    layout->setContentsMargins(0, 0, 0, 0);

    widget->setProperty("SceneState", QVariant::fromValue(new QnWorkbenchLayout(widget))); // ###

    int index = m_tabWidget->addTab(widget, Skin::icon(QLatin1String("decorations/square-view.png")), tr("Scene"));
    m_tabWidget->setCurrentIndex(index);
}

void MainWnd::currentTabChanged(int index)
{
    if (QWidget *widget = m_view->parentWidget()) {
        if (m_tabWidget->indexOf(widget) != -1) {
            if (QLayout *layout = widget->layout())
                layout->removeWidget(m_view);
        }
    }

    m_workbench->setLayout(0);

    if (QWidget *widget = m_tabWidget->widget(index)) {
        if (QLayout *layout = widget->layout())
            layout->addWidget(m_view);

        m_workbench->setLayout(widget->property("SceneState").value<QnWorkbenchLayout *>()); // ###
        m_display->fitInView(false);

        /* This one is important. If we don't unset the transformation anchor, viewport position will be messed up when show event is delivered. */
        m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
    }
}

void MainWnd::closeTab(int index)
{
    if (m_tabWidget->count() == 1)
        return; // don't close last tab

    if (QWidget *widget = m_tabWidget->widget(index)) {
        QnWorkbenchLayout *layout = widget->property("SceneState").value<QnWorkbenchLayout *>(); // ###
        if (widget->close()) {
            if (m_tabWidget->currentIndex() == index)
                m_workbench->setLayout(0);

            delete layout;

            m_tabWidget->removeTab(index);
        }
    }
}

#if 0
void MainWnd::addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout)
{
    if (files.isEmpty())
        return;

    cl_log.log(QLatin1String("Entering addFilesToCurrentOrNewLayout"), cl_logALWAYS);

    QnResourceList rlst = QnResourceDirectoryBrowser::instance().checkFiles(files);
    qnResPool->addResources(rlst);

    // If current content created by opening files or DND, use it. Otherwise create new one.
    LayoutContent* content = m_normalView->getView().getCamLayOut().getContent();

    if (!forceNewLayout && content != CLSceneLayoutManager::instance().getSearchLayout()
        && content != CLSceneLayoutManager::instance().startScreenLayoutContent())
    {
        cl_log.log(QLatin1String("Using old layout, content ") + content->getName(), cl_logALWAYS);
        foreach (const QString &file, files)
        {
            m_normalView->getView().getCamLayOut().addDevice(file, true);
            content->addDevice(file);
        }

        m_normalView->getView().fitInView(600, 100, SLOW_START_SLOW_END);
    }
    else
    {
        cl_log.log(QLatin1String("Creating new layout, content ") + content->getName(), cl_logALWAYS);
        content = CLSceneLayoutManager::instance().getNewEmptyLayoutContent();

        foreach (const QString &file, files)
            content->addDevice(file);

        m_normalView->goToNewLayoutContent(content);
    }
}

void MainWnd::goToNewLayoutContent(LayoutContent* newl)
{
    m_normalView->goToNewLayoutContent(newl);
}

void MainWnd::destroyNavigator(CLLayoutNavigator *&nav)
{
    if (nav)
    {
        nav->destroy();
        delete nav;
        nav = 0;
    }
}
#endif

void MainWnd::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
#if 0
    addFilesToCurrentOrNewLayout(files);
#else
    const QPoint gridPos = m_controller->display()->mapViewportToGrid(m_controller->display()->view()->viewport()->geometry().center());
    m_controller->drop(files, gridPos);
#endif

    activate();
}

void MainWnd::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);

    if (event->isAccepted())
        Q_EMIT mainWindowClosed();
}

void MainWnd::activate()
{
    if (isFullScreen())
        showFullScreen();
    else
        showNormal();
    raise();
    activateWindow();
}

void MainWnd::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        resize(600, 400);
    } else {
        showFullScreen();
    }
}

void MainWnd::editPreferences()
{
    PreferencesWindow dialog(this);
    dialog.exec();
}

void MainWnd::appServerError(int error)
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
        // appServerAuthenticationRequired();
        break;

    default:
        break;
    }
}

void MainWnd::appServerAuthenticationRequired()
{
    static LoginDialog *dialog = 0;
    if (dialog)
        return;

    const QUrl lastUsedUrl = Settings::lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    dialog = new LoginDialog(this);
    dialog->setModal(true);
    if (dialog->exec()) {
        const Settings::ConnectionData connection = Settings::lastUsedConnection();
        if (connection.url.isValid()) {
            QnAppServerConnectionFactory::setDefaultUrl(connection.url);

            // repopulate the resource pool
            QnResource::stopCommandProc();
            QnResourceDiscoveryManager::instance().stop();

            // don't remove local resources
            const QnResourceList localResources = qnResPool->getResourcesWithFlag(QnResource::local);
            qnResPool->clear();
            qnResPool->addResources(localResources);

            QnResourceDiscoveryManager::instance().start();
            QnResource::startCommandProc();
        }
    }
    delete dialog;
    dialog = 0;
}
