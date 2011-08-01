#include "av_singesensor.h"
#include "../dataprovider/cpul_tftp_dataprovider.h"

CLArecontSingleSensorDevice::CLArecontSingleSensorDevice(const QString& name):
m_hastestPattern(true)
{
    setName(name);
}

bool CLArecontSingleSensorDevice::getDescription()
{

	char buff[65];
	buff[64] = 0;

	for(int i = 0; i < 32; i++)
	{
		int val;

		if (getRegister(3,143+i,val)!=CL_HTTP_SUCCESS)
			return false;

		buff[2*i] = (val >> 8) & 0xFF;
		buff[2*i+1] = val & 0xFF; // 31*2+1 = 63

		if(buff[2*i] == 0 || buff[2*i+1]==0)
			break;
	}

	m_description = buff;

	return true;
}

QnAbstractMediaStreamDataProvider* CLArecontSingleSensorDevice::createMediaProvider()
{
	cl_log.log("Creating streamreader for ", getHostAddress().toString(), cl_logDEBUG1);
	return new AVClientPullSSTFTPStreamreader(QnResourcePtr(this));
}

bool CLArecontSingleSensorDevice::hasTestPattern() const
{
	return m_hastestPattern;
}
