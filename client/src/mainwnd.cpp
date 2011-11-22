#include "mainwnd.h"

#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/layout_navigator.h>
#include <ui/video_cam_layout/layout_manager.h>
#include <ui/video_cam_layout/start_screen_content.h>
#include <QtGui/QDockWidget>
#include <QtGui/QTabWidget>
#include <QtGui/QToolBar>

#include "ui/context_menu_helper.h"
#include "ui/navigationtreewidget.h"

#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/blue_background_painter.h>
#include <ui/graphics/instruments/archivedropinstrument.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_manager.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include "file_processor.h"
#include "settings.h"
#include "version.h"


MainWnd *MainWnd::s_instance = 0;

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags),
      m_normalView(0)
{
    if(s_instance != NULL)
        qnWarning("Several instances of main window created, expect problems.");
    else
        s_instance = this;
    
    /* Set up QWidget. */
    setWindowTitle(APPLICATION_NAME);
    setAttribute(Qt::WA_QuitOnClose);
    setAutoFillBackground(false);
    setAcceptDrops(true);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), app_bkr_color);
    setPalette(pal);

    const int min_width = 800;
    setMinimumWidth(min_width);
    setMinimumHeight(min_width * 3 / 4);

    /* Set up scene & view. */
    QGraphicsScene *scene = new QGraphicsScene(this);
    QnGraphicsView *view = new QnGraphicsView(scene, this);

    m_backgroundPainter.reset(new QnBlueBackgroundPainter(120.0));
    view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);

    /* Set up model & control machinery. */
    QnWorkbenchLayout *model = new QnWorkbenchLayout(this);
   
    const QSizeF defaultCellSize = QSizeF(150.0, 100.0);
    const QSizeF defaultSpacing = QSizeF(25.0, 25.0);
    QnWorkbench *state = new QnWorkbench(this);
    state->setLayout(model);
    state->mapper()->setCellSize(defaultCellSize);
    state->mapper()->setSpacing(defaultSpacing);

    QnWorkbenchManager *display = new QnWorkbenchManager(this);
    display->setScene(scene);
    display->setView(view);
    display->setWorkbench(state);

    QnWorkbenchController *controller = new QnWorkbenchController(display, this);

    /* Process input files. */
    QStringList files;
    for (int i = 1; i < argc; ++i)
        controller->drop(fromNativePath(QString::fromLocal8Bit(argv[i])), QPoint(0, 0));

    /* Prepare UI. */
    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->addTab(view, QIcon(), tr("Scene"));
    tabWidget->addTab(new QWidget(tabWidget), QIcon(), tr("Grid/Properies"));
    // Can't set 0,0,0,0 on Windows as in fullScreen mode context menu becomes invisible
    // QT bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556
    tabWidget->setContentsMargins(0, 1, 0, 0);
    tabWidget->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(tabWidget);

    // dock widgets
    NavigationTreeWidget *navigationWidget = new NavigationTreeWidget(tabWidget);

    connect(navigationWidget, SIGNAL(activated(uint)), this, SLOT(itemActivated(uint)));

    QDockWidget *navigationDock = new QDockWidget(tr("Navigation"), this);
    navigationDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    navigationDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    navigationDock->setWidget(navigationWidget);
    addDockWidget(Qt::LeftDockWidgetArea, navigationDock);


    // toolbars
    QToolBar *toolBar = new QToolBar(this);
    toolBar->addAction(&cm_exit);
    toolBar->addAction(&cm_toggle_fullscreen);
    toolBar->addAction(&cm_preferences);
    addToolBar(Qt::TopToolBarArea, toolBar);

#if 0
    if (!files.isEmpty())
        show();
    else
        showFullScreen();
#endif
}

MainWnd::~MainWnd()
{
    destroyNavigator(m_normalView);
}

void MainWnd::itemActivated(uint resourceId)
{
    // ### rewrite from scratch ;)
    QnResourcePtr resource = qnResPool->getResourceById(QnId(QString::number(resourceId)));
    if (resource && resource->checkFlag(QnResource::url))
    {
        const QString file = resource->getUrl();

        m_normalView->getView().getCamLayOut().addDevice(file, true);
        m_normalView->getView().getCamLayOut().getContent()->addDevice(file);
        m_normalView->getView().fitInView(600, 100, SLOW_START_SLOW_END);
    }
}

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

void MainWnd::handleMessage(const QString& message)
{
    QStringList files = message.trimmed().split(QLatin1Char('\0'), QString::SkipEmptyParts);

    addFilesToCurrentOrNewLayout(files);
    activate();
}

void MainWnd::closeEvent(QCloseEvent *e)
{
    QMainWindow::closeEvent(e);

    destroyNavigator(m_normalView);
}

void MainWnd::destroyNavigator(CLLayoutNavigator*& nav)
{
    if (nav)
    {
        nav->destroy();
        delete nav;
        nav = 0;
    }
}

#if 0
void MainWnd::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWnd::dropEvent(QDropEvent *event)
{
    QStringList files;
    foreach (const QUrl &url, event->mimeData()->urls())
        findAcceptedFiles(files, url.toLocalFile());

    addFilesToCurrentOrNewLayout(files, event->keyboardModifiers() & Qt::AltModifier);
    activate();
}
#endif

void MainWnd::activate()
{
    if (isFullScreen())
        showFullScreen();
    else
        showNormal();
    raise();
    activateWindow();
}
