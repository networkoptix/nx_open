#include "file_device.h"
#include "../streamreader/single_shot_file_reader.h"

CLFileDevice::CLFileDevice(QString filename)
{
	setUniqueId(filename);
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