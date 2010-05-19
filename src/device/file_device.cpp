#include "file_device.h"
#include "../streamreader/single_shot_file_reader.h"

CLFileDevice::CLFileDevice(QString filename)
{
	setUniqueId(filename);
}

QString CLFileDevice::getFileName() const
{
	return getUniqueId();
}

QString CLFileDevice::toString() const 
{
	return getUniqueId();
}


CLStreamreader* CLFileDevice::getDeviceStreamConnection()
{
	return new CLSingleShotFileStreamreader(this);
}