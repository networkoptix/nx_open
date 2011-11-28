#include "mainwnd.h"

#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/layout_navigator.h>
#include <ui/video_cam_layout/layout_manager.h>
#include <ui/video_cam_layout/start_screen_content.h>
#include <QtGui/QDockWidget>
#include <QtGui/QTabWidget>
#include <QtGui/QToolBar>

#include <ui/context_menu_helper.h>
#include <ui/navigationtreewidget.h>

#include <ui/mixins/sync_play_mixin.h>
#include <ui/mixins/render_watch_mixin.h>

#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/blue_background_painter.h>
#include <ui/graphics/instruments/archivedropinstrument.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include "file_processor.h"
#include "settings.h"
#include "version.h"


MainWnd *MainWnd::s_instance = 0;
MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags),
      m_normalView(NULL),
      m_controller(NULL)
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
   
    const QSizeF defaultCellSize = QSizeF(150.0, 100.0);
    const QSizeF defaultSpacing = QSizeF(25.0, 25.0);
    QnWorkbench *workbench = new QnWorkbench(this);
    workbench->mapper()->setCellSize(defaultCellSize);
    workbench->mapper()->setSpacing(defaultSpacing);

    QnWorkbenchDisplay *display = new QnWorkbenchDisplay(workbench, this);
    display->setScene(scene);
    display->setView(view);

    m_controller = new QnWorkbenchController(display, this);

    new QnSyncPlayMixin(display, this);

    /* Process input files. */
    QStringList files;
    for (int i = 1; i < argc; ++i)
        m_controller->drop(fromNativePath(QString::fromLocal8Bit(argv[i])), QPoint(0, 0));

    /* Prepare UI. */
    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->addTab(view, QIcon(), tr("Scene"));
    tabWidget->addTab(new QWidget(tabWidget), QIcon(), tr("Grid/Properies"));
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
    //toolBar->addActions(m_normalView->getView().actions());
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
    m_instance = 0;

    destroyNavigator(m_normalView);
}

void MainWnd::findAcceptedFiles(QStringList& files, const QString& path)
{
    if (CLAviDvdDevice::isAcceptedUrl(path))
    {
        if (path.indexOf(QLatin1Char('?')) == -1)
        {
            // open all titles on DVD
            QStringList titles = QnAVIDvdArchiveDelegate::getTitleList(path);
            foreach (const QString &title, titles)
                files << path + QLatin1String("?title=") + title;
        }
        else
        {
            files.append(path);
        }
    }
    else if (CLAviBluRayDevice::isAcceptedUrl(path))
    {
        files.append(path);
    }
    else
    {
        FileTypeSupport fileTypeSupport;
        QFileInfo fileInfo(path);
        if (fileInfo.isDir())
        {
            QDirIterator iter(path, QDirIterator::Subdirectories);
            while (iter.hasNext())
            {
                QString nextFilename = iter.next();
                if (QFileInfo(nextFilename).isFile())
                {
                    if (fileTypeSupport.isFileSupported(nextFilename))
                        files.append(nextFilename);
                }
            }
        }
        else if (fileInfo.isFile())
        {
            if (fileTypeSupport.isFileSupported(path))
                files.append(path);
        }
    }
}

void MainWnd::itemActivated(uint resourceId)
{
    // ### rewrite from scratch ;)
    QnResourcePtr resource = qnResPool->getResourceById(QnId(QString::number(resourceId)));
    
    QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
    if (!mediaResource.isNull() && m_controller->layout()->items(mediaResource->getUniqueId()).empty()) {
        QPoint gridPos = m_controller->display()->mapViewportToGrid(m_controller->display()->view()->viewport()->geometry().center());
        
        m_controller->drop(resource, gridPos);
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

void MainWnd::handleMessage(const QString &message)
{
    const QStringList files = message.split(QLatin1Char('\0'), QString::SkipEmptyParts);

    addFilesToCurrentOrNewLayout(files);
    activate();
}

void MainWnd::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);

    if (event->isAccepted())
    {
        destroyNavigator(m_normalView);

        Q_EMIT mainWindowClosed();
    }
}

}

void MainWnd::destroyNavigator(CLLayoutNavigator *&nav)
{
    if (nav)
    {
        nav->destroy();
        delete nav;
        nav = 0;
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
