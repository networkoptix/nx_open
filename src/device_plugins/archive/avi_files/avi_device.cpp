#include "avi_device.h"
#include "avi_strem_reader.h"

CLAviDevice::CLAviDevice(const QString& file)
{
    QFileInfo fi(file);

	m_uniqueId = fi.absoluteFilePath();
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
