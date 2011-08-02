#include "single_shot_dataprovider.h"

QnSingleShotStreamreader::QnSingleShotStreamreader(QnResourcePtr res):
QnAbstractMediaStreamDataProvider(res)
{
}

void QnSingleShotStreamreader::run()
{

	CL_LOG(cl_logINFO) cl_log.log("single shot stream reader started.", cl_logINFO);

	QnAbstractDataPacketPtr data (getNextData());

    if (data)
	    putData(data);

	CL_LOG(cl_logINFO) cl_log.log("single shot stream reader stopped.", cl_logINFO);
}

