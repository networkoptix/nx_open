//#include <vld.h>
#include "version.h"
#include "util.h"
#include "mainwnd.h"

#include "device/asynch_seacher.h"
#include "base/log.h"
#include "decoders/video/ipp_h264_decoder.h"
#include "device_plugins/arecontvision/devices/av_device_server.h"
#include "device_plugins/fake/devices/fake_device_server.h"

#include "ui/device_settings/dlg_factory.h"
#include "ui/device_settings/plugins/arecontvision/arecont_dlg.h"
#include "device/device_managmen/device_manager.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/context_menu_helper.h"
#include "decoders/video/abstractdecoder.h"

QMutex global_ffmpeg_mutex;

void decoderLogCallback(void* /*pParam*/, int i, const char* szFmt, va_list args)
{
	//USES_CONVERSION;

	//Ignore debug and info (i == 2 || i == 1) messages
	if(AV_LOG_ERROR != i)
	{
		//return;
	}

	// AVCodecContext* pCtxt = (AVCodecContext*)pParam;

	char szMsg[1024];
	vsprintf(szMsg, szFmt, args);
	//if(szMsg[strlen(szMsg)] == '\n')
	{	
		szMsg[strlen(szMsg)-1] = 0;
	}

	cl_log.log("FFMPEG ", szMsg, cl_logERROR);
}

int main(int argc, char *argv[])
{
//	av_log_set_callback(decoderLogCallback);

    QApplication::setOrganizationName(ORGANIZATION_NAME);
    QApplication::setApplicationName(APPLICATION_NAME);
    QApplication::setApplicationVersion(APPLICATION_VERSION);

    QString dataLocation = getDataDirectory();

	QApplication a(argc, argv);

	QDir::setCurrent(QFileInfo(argv[0]).absolutePath());

	QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + "/log");

	if (!cl_log.create(dataLocation + "/log/log_file", 1024*1024*10, 5, cl_logDEBUG1))
    {
		a.quit();

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
		cl_log.log("Software version 0.1", cl_logALWAYS);
		cl_log.log(argv[0], cl_logALWAYS);
	}

	QString rootDir = getMediaRootDir();
    cl_log.log("Using " + rootDir + " as media root directory", cl_logALWAYS);

	CLDevice::startCommandProc();

	CLDeviceManager::instance(); // to initialize net state;

	//===========================================================================
	//IPPH264Decoder::dll.init();

	CLVideoDecoderFactory::setCodecManufacture(CLVideoDecoderFactory::FFMPEG);

	//============================
	CLDeviceManager::instance().getDiveceSercher().addDeviceServer(&AVDeviceServer::instance());
	CLDeviceManager::instance().getDiveceSercher().addDeviceServer(&FakeDeviceServer::instance());

	CLDeviceSettingsDlgFactory::instance().registerDlgManufacture(&AreconVisionDlgManufacture::instance());
	//============================

	//=========================================================

	qApp->setStyleSheet("\
		QMenu {\
		background-color: black;\
		color: white;\
		font-family: Bodoni MT;\
		font-size: 18px;\
		border: 1px solid rgb(110, 110, 110);\
		}\
		QMenu::item{\
		padding-top: 4px;\
		padding-left: 5px;\
		padding-right: 15px;\
		padding-bottom: 4px;\
		}\
		QMenu::item:selected {\
		background: rgb(40, 40, 40);\
		}");

	/**/
	//=========================================================

	CLDeviceList recorders = CLDeviceManager::instance().getRecorderList();
	foreach(CLDevice* dev, recorders)
	{
		QString id = dev->getUniqueId();
		CLSceneLayoutManager::instance().addRecorderLayoutContent(id);
		dev->releaseRef();
	}

	//=========================================================

	initContextMenu();

	MainWnd w;
	w.show();
	return a.exec();

	CLDevice::stopCommandProc();
}
