#include "common/log.h"
#include "resource/resource.h"
#include "resources/avigilon/resoutce/avigilon_resource_seaecher.h"
#include "resourcecontrol/resource_pool.h"
#include "resources/arecontvision/resource/av_resource_searcher.h"
#include "resources/iqeye/resource/iqeye_resource_searcher.h"
#include "resources/android/resource/android_resource_searcher.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"

 
int main(int argc, char *argv[])
{

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

	//===========================================================================
	//IPPH264Decoder::dll.init();



	//============================
	//QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&QnPlArecontResourceSearcher::instance());
    //QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&AVigilonDeviceServer::instance());
    //QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&AndroidDeviceServer::instance());
    //QnResourcePool::instance().getDeviceSearcher().addDeviceServer(&IQEyeDeviceServer::instance());

	//=========================================================
    app.exec();

	QnResource::stopCommandProc();
}
