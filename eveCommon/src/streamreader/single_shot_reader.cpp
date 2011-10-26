#include "./single_shot_reader.h"
#include "../base/log.h"
#include "../data/mediadata.h"
#include "./device/qnresource.h"

CLSingleShotStreamreader::CLSingleShotStreamreader(QnResource* dev ):
QnAbstractStreamDataProvider(dev)
{
	dev->addFlag(QnResource::SINGLE_SHOT);
}

void CLSingleShotStreamreader::run()
{

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("single shot stream reader started."), cl_logINFO);

	QnAbstractDataPacketPtr data = getData();

	if (data)
		putData(data);

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("single shot stream reader stopped."), cl_logINFO);
}

