#include "av_resource_searcher.h"
#include "av_resource.h"
#include "../tools/AVJpegHeader.h"


extern const char* ArecontVisionManufacture;

QnPlArecontResourceSearcher::QnPlArecontResourceSearcher()
{
	// everything related to Arecont must be initialized here
	AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");

	QString error;
	if (QnPlAreconVisionResource::loadDevicesParam(QCoreApplication::applicationDirPath() + "/arecontvision/devices.xml", error))
	{
		CL_LOG(cl_logINFO)
		{
			QString msg;
			QTextStream str(&msg) ;
			QStringList lst = QnResource::supportedDevises();
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


QString QnPlArecontResourceSearcher::manufacture() const
{
	return ArecontVisionManufacture;
}

// returns all available devices 
QnResourceList QnPlArecontResourceSearcher::findResources()
{
	return QnPlAreconVisionResource::findDevices();
}

QnPlArecontResourceSearcher& QnPlArecontResourceSearcher::instance()
{
	static QnPlArecontResourceSearcher inst;
	return inst;
}

QnResourcePtr QnPlArecontResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}