#include "av_device_server.h"
#include "av_device.h"
#include "../tools/AVJpegHeader.h"
#include "../../../base/log.h"

AVDeviceServer::AVDeviceServer()
{
	// everything related to Arecont must be initialized here
	AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");

	QString error;
	if (CLAreconVisionDevice::loadDevicesParam(QCoreApplication::applicationDirPath() + QLatin1String("/arecontvision/devices.xml"), error))
	{
		CL_LOG(cl_logINFO)
		{
			QString msg;
			QTextStream str(&msg) ;
			QStringList lst = CLDevice::supportedDevises();
			str << QLatin1String("Ssupported devices loaded; size = ") << lst.size() << QLatin1String(": ") << endl << lst.join(QLatin1String("\n"));
			cl_log.log(msg, cl_logINFO);
		}
	}
	else
	{
		CL_LOG(cl_logERROR)
		{
			QString log = QLatin1String("Cannot load devices list. Error:");
			log += error;
			cl_log.log(log, cl_logERROR);
		}

		cl_log.log(QLatin1String("Cannot load arecontvision/devices.xml"), cl_logERROR);
		QMessageBox msgBox;
		msgBox.setText(QMessageBox::tr("Error"));
		msgBox.setInformativeText(QMessageBox::tr("Cannot load /arecontvision/devices.xml"));
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.setDefaultButton(QMessageBox::Ok);
		msgBox.setIcon(QMessageBox::Warning);
		msgBox.exec();
	}
}

bool AVDeviceServer::isProxy() const
{
	return false; // this not actualy a server; this is just a cameras
}

QString AVDeviceServer::name() const
{
	return QLatin1String("Arecont Vision");
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
