#include "resource/resource.h"
#include "single_shot_dataprovider.h"
#include "streamdataprovider.h"
#include "datapacket/mediadatapacket.h"

CLSingleShotStreamreader::CLSingleShotStreamreader(QnResource* dev ):
QnStreamDataProvider(dev)
{
	dev->addDeviceTypeFlag(QnResource::SINGLE_SHOT);
}

void CLSingleShotStreamreader::run()
{

	CL_LOG(cl_logINFO) cl_log.log("single shot stream reader started.", cl_logINFO);

	QnAbstractMediaDataPacketPtr data (getData());

    if (data)
	    putData(data);

	CL_LOG(cl_logINFO) cl_log.log("single shot stream reader stopped.", cl_logINFO);
}

