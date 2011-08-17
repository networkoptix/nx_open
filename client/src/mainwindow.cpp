#include "mainwindow.h"

#include <QtGui/QToolBar>

#include "graphicsview.h"

// ### removeme
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include "resource/video_server.h"
#include "resource/resource.h"
#include "resource/client/client_media_resource.h"
#include "resourcecontrol/resource_pool.h"
#include "camera/camdisplay.h"
#include "camera/gl_renderer.h"
#include "videoitem/video_wnd_item.h"

static const QnId TEST_RES_ID("1001");
// ###

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("VMS Client"));

    GraphicsView *view = new GraphicsView(this);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->addActions(view->actions());
    addToolBar(Qt::TopToolBarArea, toolBar);

    setCentralWidget(view);

    showMaximized();

// ### removeme
    QnVideoServer *server = new QnVideoServer();
    server->setUrl("rtsp://localhost:50000/");
    QnResourcePool::instance().addResource(QnResourcePtr(server));

    QnClientMediaResource *resource = new QnClientMediaResource();
    resource->setId(TEST_RES_ID+1);
    resource->setUrl("TC-Outro.mov");
    resource->setParentId(server->getId());
    QnResourcePool::instance().addResource(QnResourcePtr(resource));

    QTimer::singleShot(500, this, SLOT(boo()));
// ###
}

// ### removeme
void MainWindow::boo()
{
    QnResourcePtr resource = QnResourcePool::instance().getResourceById(TEST_RES_ID+1);
    //if (resource->checkFlag(QnResource::playback | QnResource::media | QnResource::video))
    if (QnClientMediaResourcePtr media = resource.dynamicCast<QnClientMediaResource>())
    {
        QnAbstractMediaStreamDataProvider *msdp = media->createMediaProvider();
        CLVideoWindowItem *vwi = new CLVideoWindowItem(media->getMediaLayout(), 0, 0, media->getName());
        CLAbstractRenderer *renderer = new CLGLRenderer(vwi);
        CLCamDisplay *camDisplay = new CLCamDisplay;
        camDisplay->addVideoChannel(0, renderer, false);
        msdp->addDataProcessor(camDisplay);
        msdp->start();
        camDisplay->start();
    }
}
// ###
