#include "mainwnd.h"

#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

#include <QtGui/QDockWidget>
#include <QtGui/QTabWidget>

#include "ui/context_menu_helper.h"
#include "ui/layout_navigator.h"
#include "ui/navigationtreewidget.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"

#include "version.h"
#include "plugins/resources/archive/avi_files/avi_dvd_device.h"
#include "plugins/resources/archive/avi_files/avi_dvd_archive_delegate.h"
#include "plugins/resources/archive/avi_files/avi_bluray_device.h"
#include "plugins/resources/archive/filetypesupport.h"
#include "core/resource/directory_browser.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"

MainWnd* MainWnd::m_instance = 0;

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

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags),
      m_normalView(0)
{
    m_instance = this;

    setWindowTitle(APPLICATION_NAME);

    setAttribute(Qt::WA_QuitOnClose);

    setAutoFillBackground(false);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), app_bkr_color);
    setPalette(pal);

    //setWindowOpacity(.80);

    setAcceptDrops(true);

    //=======================================================

    const int min_width = 800;
    setMinimumWidth(min_width);
    setMinimumHeight(min_width*3/4);

    QStringList files;
    for (int i = 1; i < argc; ++i)
        findAcceptedFiles(files, fromNativePath(QString::fromLocal8Bit(argv[i])));

    LayoutContent* content = 0;

    if (!files.isEmpty())
    {
        QnResourceList rlst = QnResourceDirectoryBrowser::instance().checkFiles(files);
        qnResPool->addResources(rlst);

        content = CLSceneLayoutManager::instance().getNewEmptyLayoutContent();

        foreach (const QString &file, files)
            content->addDevice(file);
    }

    //=======add====
    m_normalView = new CLLayoutNavigator(this, content);

    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->addTab(&m_normalView->getView(), QIcon(), tr("Scene"));
    tabWidget->addTab(new QWidget(tabWidget), QIcon(), tr("Grid/Properies"));
#ifdef Q_OS_WIN
    // Can't set 0,0,0,0 on Windows as in fullScreen mode context menu becomes invisible
    // QT bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556
    tabWidget->setContentsMargins(0, 1, 0, 0);
#else
    tabWidget->setContentsMargins(0, 0, 0, 0);
#endif
    setCentralWidget(tabWidget);

    m_normalView->setMode(NORMAL_ViewMode);
    m_normalView->getView().setViewMode(GraphicsView::NormalView);


    // dock widgets
    NavigationTreeWidget *navigationWidget = new NavigationTreeWidget(tabWidget);

    connect(navigationWidget, SIGNAL(activated(uint)), this, SLOT(itemActivated(uint)));

    QDockWidget *navigationDock = new QDockWidget(tr("Navigation"), this);
    navigationDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    navigationDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    navigationDock->setWidget(navigationWidget);
    addDockWidget(Qt::LeftDockWidgetArea, navigationDock);


    if (!files.isEmpty())
        show();
    else
        showFullScreen();
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

void MainWnd::activate()
{
    if (isFullScreen())
        showFullScreen();
    else
        showNormal();
    raise();
    activateWindow();
}
