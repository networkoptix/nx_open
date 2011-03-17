#include "avi_device.h"
#include "avi_strem_reader.h"

CLAviDevice::CLAviDevice(const QString& file)
{
	m_uniqueId = file;
	m_name = QFileInfo(file).fileName();
}

CLAviDevice::~CLAviDevice()
{

}

QString CLAviDevice::toString() const
{
	return m_name;
}

CLStreamreader* CLAviDevice::getDeviceStreamConnection()
{
	return new CLAVIStreamReader(this);
}