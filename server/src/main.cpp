#include "common/log.h"
#include "resource/resource.h"
#include "resourcecontrol/resource_pool.h"
#include "resources/arecontvision/resource/av_resource_searcher.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"
#include "rtsp/rtsp_listener.h"
#include "resource/media_resource.h"
#include "resource/security_cam_resource.h"
#include "resources/archive/archive_resource.h"
#include "resource/client/client_media_resource.h"
#include "resource/video_server.h"

static const QnId TEST_RES_ID("1001");

class TestThread: public QThread
{
protected:
    virtual void run()
    {
        QnVideoServer* server = new QnVideoServer();
        server->setUrl("rtsp://localhost:50000");
        QnResourcePool::instance().addResource(QnResourcePtr(server));
        QnClientMediaResource* camera = new QnClientMediaResource();
        camera->setId(TEST_RES_ID);
        camera->setParentId(server->getId());
        QnResourcePool::instance().addResource(QnResourcePtr(camera));

        QnAbstractMediaStreamDataProvider* dp = camera->createMediaProvider();
        while (1)
        {
            QnAbstractDataPacketPtr data = dp->getNextData();
        }
    }
};
 
int main(int argc, char *argv[])
{
    QnRtspListener listener();

    QCoreApplication app(argc, argv);

    QDir::setCurrent(QFileInfo(argv[0]).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkdir("./log");


	if (!cl_log.create("./log/log_file", 1024*1024*10, 5, cl_logDEBUG1))
    {
        return 0;
    }

#ifdef _DEBUG
	 cl_log.setLogLevel(cl_logDEBUG1);
	//cl_log.setLogLevel(cl_logWARNING);
#else
	cl_log.setLogLevel(cl_logWARNING);
#endif

	CL_LOG(cl_logALWAYS)
	{
		cl_log.log("\n\n========================================", cl_logALWAYS);
		cl_log.log(cl_logALWAYS, "Software version %s", 0);
		cl_log.log(argv[0], cl_logALWAYS);
	}



	QnResource::startCommandProc();

	QnResourcePool::instance(); // to initialize net state;

    //QnArchiveResource* res = new QnArchiveResource("e:/Users/roman76r/video/ROCKNROLLA/BDMV/STREAM/00000.m2ts");
    QnArchiveResource* res = new QnArchiveResource("test.flv");
    res->setId(TEST_RES_ID);
    QnResourcePool::instance().addResource(QnResourcePtr(res));

    TestThread* testThread = new TestThread();
    testThread->start();
	//===========================================================================
	//IPPH264Decoder::dll.init();



	//============================
	//QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&QnPlArecontResourceSearcher::instance());
    //QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&AVigilonDeviceServer::instance());
    //QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&AndroidDeviceServer::instance());
    //QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&IQEyeDeviceServer::instance());

	//=========================================================
    QnRtspListener rtspServer(QHostAddress::Any, 50000);

    app.exec();

	QnResource::stopCommandProc();
}
