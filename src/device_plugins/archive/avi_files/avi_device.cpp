#include "avi_device.h"
#include "avi_strem_reader.h"


CLAviDevice::CLAviDevice(const QString& file)
{
	m_uniqueid = file;
	m_name = file;
}

CLAviDevice::~CLAviDevice()
{

}

CLStreamreader* CLAviDevice::getDeviceStreamConnection()
{
	return new CLAVIStreamReader(this);
}