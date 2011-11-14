#include "mainwnd.h"

#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/layout_navigator.h>
#include <ui/video_cam_layout/layout_manager.h>
#include <ui/video_cam_layout/start_screen_content.h>

#include <ui/view/graphics_view.h>
#include <ui/view/blue_background_painter.h>

#include <ui/instruments/archivedropinstrument.h>

#include <ui/model/ui_layout.h>
#include <ui/scene/scene_controller.h>
#include <ui/scene/display_synchronizer.h>
#include <ui/scene/display_state.h>

#include <utils/common/util.h>
#include <utils/common/warnings.h>

#include "file_processor.h"
#include "settings.h"
#include "version.h"


MainWnd *MainWnd::s_instance = 0;

MainWnd::MainWnd(int argc, char* argv[], QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags),
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

    /* Set up UI. */
    QGraphicsScene *scene = new QGraphicsScene(this);
    QnGraphicsView *view = new QnGraphicsView(scene, this);

    QHBoxLayout *layout = new QHBoxLayout(this);
    /* Can't set 0,0,0,0 on Windows as in fullScreen mode context menu becomes invisible.
     * Looks like a QT bug: http://bugreports.qt.nokia.com/browse/QTBUG-7556 */
#ifdef Q_OS_WIN
    layout->setContentsMargins(0, 1, 0, 0);
#else
    layout->setContentsMargins(0, 0, 0, 0);
#endif
    layout->addWidget(view);

    m_backgroundPainter.reset(new QnBlueBackgroundPainter(120.0));
    view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);

    /* Set up model & control machinery. */
    QnUiLayout *uiLayout = new QnUiLayout(this);
    QnDisplayState *state = new QnDisplayState(uiLayout, this);
    QnDisplaySynchronizer *synchronizer = new QnDisplaySynchronizer(state, scene, view, this);
    QnSceneController *controller = new QnSceneController(synchronizer, this);

    /* Process input files. */
    QStringList files;
    for (int i = 1; i < argc; ++i)
        controller->archiveDropInstrument()->drop(view, QUrl::fromLocalFile(fromNativePath(QString::fromLocal8Bit(argv[i]))), view->rect().center());

    /*
    QnFileProcessor::findAcceptedFiles(, &files);

    if (!files.isEmpty())
    {
        QnResourceList rlst = QnResourceDirectoryBrowser::instance().checkFiles(files);
        qnResPool->addResources(rlst);

        content = CLSceneLayoutManager::instance().getNewEmptyLayoutContent();

        foreach (const QString &file, files)
            content->addDevice(file);
    }
    */

    /*
    //=======add====
    //m_normalView = new CLLayoutNavigator(this, content);

    //m_normalView->setMode(NORMAL_ViewMode);
    //m_normalView->getView().setViewMode(GraphicsView::NormalView);
    */

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
    QWidget::closeEvent(e);

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
