#include "avi_resource.h"
#include "avi_strem_dataprovider.h"

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

QnStreamDataProvider* CLAviDevice::getDeviceStreamConnection()
{
	return new QnPlAVIStreamProvider(this);
}
