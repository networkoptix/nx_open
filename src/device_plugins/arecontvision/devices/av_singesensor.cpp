#include "av_singesensor.h"
#include "../streamreader/cpul_tftp_stremreader.h"
#include "../../../base/log.h"

CLArecontSingleSensorDevice::CLArecontSingleSensorDevice(int model):
CLAreconVisionDevice(model),
m_hastestPattern(true)
{

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

	m_description = QString::fromAscii(buff);

	return true;
}

CLStreamreader* CLArecontSingleSensorDevice::getDeviceStreamConnection()
{
	cl_log.log(QLatin1String("Creating streamreader for "), getIP().toString(), cl_logDEBUG1);
	return new AVClientPullSSTFTPStreamreader(this);
}

bool CLArecontSingleSensorDevice::hasTestPattern() const
{
	return m_hastestPattern;
}
