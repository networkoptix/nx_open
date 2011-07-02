#include "av_resource_searcher.h"
#include "av_resource.h"
#include "../tools/AVJpegHeader.h"


AVDeviceServer::AVDeviceServer()
{
	// everything related to Arecont must be initialized here
	AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");

	QString error;
	if (CLAreconVisionDevice::loadDevicesParam(QCoreApplication::applicationDirPath() + "/arecontvision/devices.xml", error))
	{
		CL_LOG(cl_logINFO)
		{
			QString msg;
			QTextStream str(&msg) ;
			QStringList lst = CLDevice::supportedDevises();
			str << "Ssupported devices loaded; size = " << lst.size() << ": " << endl << lst.join("\n");
			cl_log.log(msg, cl_logINFO);
		}
	}
	else
	{
		CL_LOG(cl_logERROR)
		{
			QString log  = "Cannot load devices list. Error:";
			log+=error;
			cl_log.log(log, cl_logERROR);
		}

	}

}

bool AVDeviceServer::isProxy() const
{
	return false; // this not actualy a server; this is just a cameras
}

QString AVDeviceServer::name() const
{
	return "Arecont Vision";
}

// returns all available devices 
CLDeviceList AVDeviceServer::findDevices()
{
	return CLAreconVisionDevice::findDevices();
}

AVDeviceServer& AVDeviceServer::instance()
{
	static AVDeviceServer inst;
	return inst;
}
