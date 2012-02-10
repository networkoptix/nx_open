#include "../resource/resource.h"
#include "single_shot_dataprovider.h"


CLSingleShotStreamreader::CLSingleShotStreamreader(QnResourcePtr dev ):
QnAbstractStreamDataProvider(dev)
{
	dev->addFlags(QnResource::SINGLE_SHOT);
}

void CLSingleShotStreamreader::run()
{

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("single shot stream reader started."), cl_logINFO);

	QnAbstractDataPacketPtr data = getData();

	if (data)
		putData(data);

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("single shot stream reader stopped."), cl_logINFO);
}

