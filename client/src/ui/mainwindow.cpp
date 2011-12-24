#include "mainwindow.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QSplitter>
#include <QtGui/QStyle>
#include <QtGui/QToolBar>
#include <QtGui/QToolButton>

#include <QtNetwork/QNetworkReply>

#include <api/AppServerConnection.h>
#include <api/SessionManager.h>

#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/asynch_seacher.h>
#include <core/resourcemanagment/resource_pool.h>

#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/video_cam_layout/start_screen_content.h"

#include "ui/context_menu_helper.h"
#include "ui/navigationtreewidget.h"

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

#include "ui/widgets3/tabbar.h"

#include "ui/skin/skin.h"

#include <utils/common/util.h>
#include <utils/common/warnings.h>

#include "file_processor.h"
#include "settings.h"
#include "version.h"

MainWindow::MainWindow(int argc, char *argv[], QWidget *parent, Qt::WindowFlags flags)
    : FancyMainWindow(parent, flags | Qt::WindowShadeButtonHint),
      m_controller(0)
{
    setWindowTitle(APPLICATION_NAME);
    setAttribute(Qt::WA_QuitOnClose);
    setAcceptDrops(true);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), app_bkr_color);
    setPalette(pal);

    const int min_width = 800;
    setMinimumWidth(min_width);
    setMinimumHeight(min_width * 3 / 4);


    // set up actions
    connect(&cm_exit, SIGNAL(triggered()), this, SLOT(close()));
    addAction(&cm_exit);

    connect(&cm_toggle_fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
    addAction(&cm_toggle_fullscreen);

    connect(&cm_preferences, SIGNAL(triggered()), this, SLOT(editPreferences()));
    addAction(&cm_preferences);

    QAction *reconnectAction = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Reconnect"), this);
    connect(reconnectAction, SIGNAL(triggered()), this, SLOT(appServerAuthenticationRequired()));

    connect(SessionManager::instance(), SIGNAL(error(int)), this, SLOT(appServerError(int)));

    QAction *newTabAction = new QAction(Skin::icon(QLatin1String("plus.png")), tr("New Tab"), this);
    newTabAction->setShortcut(QKeySequence::New);
    newTabAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(newTabAction, SIGNAL(triggered()), this, SLOT(addTab()));


    // Set up scene & view
    QGraphicsScene *scene = new QGraphicsScene(this);
    m_view = new QnGraphicsView(scene, this);

    m_backgroundPainter.reset(new QnBlueBackgroundPainter(120.0));
    m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);

    // Set up model & control machinery
    const QSizeF defaultCellSize = QSizeF(150.0, 100.0);
    const QSizeF defaultSpacing = QSizeF(25.0, 25.0);
    m_workbench = new QnWorkbench(this);
    m_workbench->mapper()->setCellSize(defaultCellSize);
    m_workbench->mapper()->setSpacing(defaultSpacing);

    m_display = new QnWorkbenchDisplay(m_workbench, this);
    m_display->setScene(scene);
    m_display->setView(m_view);

    m_controller = new QnWorkbenchController(m_display, this);

    new QnSyncPlayMixin(m_display, this);

    // Prepare UI
    NavigationTreeWidget *navigationWidget = new NavigationTreeWidget(this);
    connect(navigationWidget, SIGNAL(activated(uint)), this, SLOT(itemActivated(uint)));

    m_tabBar = new TabBar(this);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);

    connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
    connect(m_tabBar, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    {
        QToolBar *toolBar = new QToolBar(this);
        toolBar->setAllowedAreas(Qt::NoToolBarArea);
        toolBar->addAction(reconnectAction);
        toolBar->addAction(&cm_preferences);
        toolBar->addAction(newTabAction);

        QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(layout());
        Q_ASSERT(mainLayout);
        QHBoxLayout *tabBarLayout = new QHBoxLayout;
        tabBarLayout->setContentsMargins(0, 0, 0, 0);
        tabBarLayout->setSpacing(2);
        tabBarLayout->addWidget(toolBar);
        tabBarLayout->addWidget(m_tabBar, 255);
        mainLayout->insertLayout(1, tabBarLayout);
    }

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(navigationWidget);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setCollapsible(0, true);
    m_splitter->addWidget(m_view);
    m_splitter->setStretchFactor(1, 99);
    setCentralWidget(m_splitter);

    // Can't set 0,0,0,0 on Windows as in fullScreen mode context menu becomes invisible
    // Looks like Qt bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556
#ifdef Q_OS_WIN
    setContentsMargins(0, 1, 0, 0);
#else
    setContentsMargins(0, 0, 0, 0);
#endif


    addTab();

    // Process input files
    const QPoint gridPos = m_controller->display()->mapViewportToGrid(m_controller->display()->view()->viewport()->geometry().center());
    for (int i = 1; i < argc; ++i)
        m_controller->drop(fromNativePath(QString::fromLocal8Bit(argv[i])), gridPos);

    showNormal();
}

MainWindow::~MainWindow()
{
}

Q_DECLARE_METATYPE(QnWorkbenchLayout *) // ###

void MainWindow::addTab()
{
    const int index = m_tabBar->addTab(Skin::icon(QLatin1String("decorations/square-view.png")), tr("Scene"));
    m_tabBar->setTabData(index, QVariant::fromValue(new QnWorkbenchLayout(m_view))); // ###
    m_tabBar->setCurrentIndex(index);
}

void MainWindow::currentTabChanged(int index)
{
    QnWorkbenchLayout *layout = m_tabBar->tabData(index).value<QnWorkbenchLayout *>(); // ###
    m_workbench->setLayout(layout);
    m_display->fitInView(false);
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
}

void MainWindow::closeTab(int index)
{
    if (m_tabBar->count() == 1)
        return; // don't close last tab

    if (m_tabBar->currentIndex() == index)
        m_workbench->setLayout(0);

    QnWorkbenchLayout *layout = m_tabBar->tabData(index).value<QnWorkbenchLayout *>(); // ###
    delete layout;

    m_tabBar->removeTab(index);
}

void MainWindow::itemActivated(uint resourceId)
{
    // ### rewrite from scratch ;)
    QnResourcePtr resource = qnResPool->getResourceById(QnId(QString::number(resourceId)));

    QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
    if (mediaResource && m_controller->layout()->items(mediaResource->getUniqueId()).isEmpty()) {
        QPoint gridPos = m_controller->display()->mapViewportToGrid(m_controller->display()->view()->viewport()->geometry().center());
        m_controller->drop(resource, gridPos);
    }
}

void MainWindow::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    const QPoint gridPos = m_controller->display()->mapViewportToGrid(m_controller->display()->view()->viewport()->geometry().center());
    m_controller->drop(files, gridPos);

    activate();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    FancyMainWindow::closeEvent(event);

    if (event->isAccepted())
        Q_EMIT mainWindowClosed();
}

void MainWindow::activate()
{
    if (isFullScreen())
        showFullScreen();
    else
        showNormal();
    raise();
    activateWindow();
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        resize(600, 400);
    } else {
        showFullScreen();
    }
}

void MainWindow::editPreferences()
{
    PreferencesWindow dialog(this);
    dialog.exec();
}

void MainWindow::appServerError(int error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError: // ### remove ?
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::UnknownNetworkError:
        appServerAuthenticationRequired();
        break;

    default:
        break;
    }
}

void MainWindow::appServerAuthenticationRequired()
{
    static LoginDialog *dialog = 0;
    if (dialog)
        return;

    const QUrl lastUsedUrl = Settings::lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    dialog = new LoginDialog(this);
    if (dialog->exec()) {
        const Settings::ConnectionData connection = Settings::lastUsedConnection();
        if (connection.url.isValid()) {
            QnAppServerConnectionFactory::setDefaultUrl(connection.url);

            // repopulate the resource pool
            QnResource::stopCommandProc();
            QnResourceDiscoveryManager::instance().stop();
            qnResPool->clear(); // ### don't remove local resources
            QnResourceDiscoveryManager::instance().start();
            QnResource::startCommandProc();
        }
    }
    delete dialog;
    dialog = 0;
}
