#include "mainwnd.h"
#include <QtGui/QApplication>

#include <QDir>
#include "device/asynch_seacher.h"
#include "base/log.h"
#include "decoders/video/ipp_h264_decoder.h"
#include "decoders/video/ffmpeg.h"
#include "device_plugins/arecontvision/devices/av_device_server.h"
#include "device_plugins/fake/devices/fake_device_server.h"
#include <QMessageBox>


CLDiviceSeracher dev_searcher;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QDir::setCurrent(QFileInfo(argv[0]).absolutePath());

	QDir current_dir;
	current_dir.mkdir("log");

	if (!cl_log.create("./log/log_file", 1024*1024*10, 5, cl_logDEBUG1))
		return a.quit();



	CL_LOG(cl_logALWAYS)
	{
		cl_log.log("\n\n========================================", cl_logALWAYS);
		cl_log.log("Software version 0.1", cl_logALWAYS);
		cl_log.log(argv[0], cl_logALWAYS);
	}

	CLDevice::startCommandProc();

	cl_log.log(dev_searcher.getNetState().toString(), cl_logALWAYS);

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
	dev_searcher.addDeviceServer(&AVDeviceServer::instance());
	//dev_searcher.addDeviceServer(&FakeDeviceServer::instance());
	//============================

	MainWnd w;
	w.show();
	return a.exec();

	CLDevice::stopCommandProc();
}
