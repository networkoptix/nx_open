//#include <vld.h>
#include "mainwnd.h"
#include <QtGui/QApplication>

#include <QDir>
#include "device/asynch_seacher.h"
#include "base/log.h"
#include "decoders/video/ipp_h264_decoder.h"
#include "decoders/video/ffmpeg.h"
#include "device_plugins/arecontvision/devices/av_device_server.h"
#include "device_plugins/fake/devices/fake_device_server.h"

#include "ui/device_settings/dlg_factory.h"
#include "ui/device_settings/plugins/arecontvision/arecont_dlg.h"
#include <QMessageBox>
#include "device/device_managmen/device_manager.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/context_menu_helper.h"


int main(int argc, char *argv[])
{

	QApplication a(argc, argv);
	QDir::setCurrent(QFileInfo(argv[0]).absolutePath());

	QDir current_dir;
	current_dir.mkdir("log");

	if (!cl_log.create("./log/log_file", 1024*1024*10, 5, cl_logDEBUG1))
		return a.quit();

	cl_log.setLogLevel(cl_logWARNING);


	CL_LOG(cl_logALWAYS)
	{
		cl_log.log("\n\n========================================", cl_logALWAYS);
		cl_log.log("Software version 0.1", cl_logALWAYS);
		cl_log.log(argv[0], cl_logALWAYS);
	}

	CLDevice::startCommandProc();

	cl_log.log(CLDeviceManager::instance().getDiveceSercher().getNetState().toString(), cl_logALWAYS);

	//===========================================================================
	//IPPH264Decoder::dll.init();
	if (!CLFFmpegVideoDecoder::dll.init())
	{
		cl_log.log("Cannot load FFMPEG dlls", cl_logERROR);
		QMessageBox msgBox;
		msgBox.setText("Error");
		msgBox.setInformativeText("Cannot load FFMPEG dlls");
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.setDefaultButton(QMessageBox::Ok);
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.exec();
		return a.quit();
	}


	CLDecoderFactory::setCodecManufacture(CLDecoderFactory::FFMPEG);

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
