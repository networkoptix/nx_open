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
#include "videoitem/video_wnd_item.h"

static const QnId TEST_RES_ID("1001");
// ###

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("VMS Client"));

    m_view = new GraphicsView(this);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->addActions(m_view->actions());
    addToolBar(Qt::TopToolBarArea, toolBar);

    setCentralWidget(m_view);

    showMaximized();

// ### removeme
    QnVideoServer *server = new QnVideoServer();
    server->setUrl("rtsp://localhost:50000");
    QnResourcePool::instance().addResource(QnResourcePtr(server));

    QnClientMediaResource *resource = new QnClientMediaResource();
    resource->setId(TEST_RES_ID+1);
    resource->setUrl("TC-Outro.mov");
    resource->setParentId(server->getId());
    QnResourcePool::instance().addResource(QnResourcePtr(resource));

    QTimer::singleShot(0, this, SLOT(boo()));
// ###
}

// ### removeme
void MainWindow::boo()
{
    QnResourcePtr resource = QnResourcePool::instance().getResourceById(TEST_RES_ID+1);
    //if (resource->checkFlag(QnResource::playback | QnResource::media | QnResource::video))
    if (QnClientMediaResourcePtr media = resource.dynamicCast<QnClientMediaResource>())
    {
        CLVideoWindowItem *vwi = new CLVideoWindowItem(media->getMediaLayout(), 300, 300, media->getName());
        CLCamDisplay *camDisplay = new CLCamDisplay;
        int videonum = media->getMediaLayout()->numberOfVideoChannels();
        for (int i = 0; i < videonum; ++i)
            camDisplay->addVideoChannel(i, vwi, false/*!singleshot*/);

        QnAbstractMediaStreamDataProvider *msdp = media->createMediaProvider();
        msdp->addDataProcessor(camDisplay);

        m_view->scene()->addItem(vwi);
        vwi->setPos(-150, 20);

        camDisplay->start();
        msdp->start();
    }
}
// ###
