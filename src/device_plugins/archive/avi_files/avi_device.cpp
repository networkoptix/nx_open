#include "avi_device.h"
#include "avi_strem_reader.h"

#include <QFileInfo>

CLAviDevice::CLAviDevice(const QString& file)
{
	m_uniqueid = file;
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