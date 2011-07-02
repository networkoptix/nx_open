#include "resource/resource.h"
#include "single_shot_dataprovider.h"
#include "streamdataprovider.h"
#include "datapacket/mediadatapacket.h"

CLSingleShotStreamreader::CLSingleShotStreamreader(CLDevice* dev ):
CLStreamreader(dev)
{
	dev->addDeviceTypeFlag(CLDevice::SINGLE_SHOT);
}

void CLSingleShotStreamreader::run()
{

	CL_LOG(cl_logINFO) cl_log.log("single shot stream reader started.", cl_logINFO);

	CLAbstractMediaData *data = 0;
	data = getData();

    if (data)
	    putData(data);

	CL_LOG(cl_logINFO) cl_log.log("single shot stream reader stopped.", cl_logINFO);
}

