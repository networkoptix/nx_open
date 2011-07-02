#include "file_resource.h"
#include "dataprovider/single_shot_file_dataprovider.h"
#include "dataprovider/streamdataprovider.h"

CLFileDevice::CLFileDevice(QString filename)
{
    QFileInfo fi(filename);
	setUniqueId(fi.absoluteFilePath());
	m_name = QFileInfo(filename).fileName();
	addDeviceTypeFlag(CLDevice::SINGLE_SHOT);
}

QString CLFileDevice::getFileName() const
{
	return getUniqueId();
}

QString CLFileDevice::toString() const 
{
	return m_name;
}

CLStreamreader* CLFileDevice::getDeviceStreamConnection()
{
	return new CLSingleShotFileStreamreader(this);
}
