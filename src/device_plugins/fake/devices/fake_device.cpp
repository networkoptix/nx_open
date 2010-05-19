#include "fake_device.h"
#include <QString>
#include "../streamreader/fake_file_streamreader.h"


CLDeviceList FakeDevice::findDevices()
{
	CLDeviceList result;

	for (int i = 0; i < 5; ++i)
	{
		CLDevice* dev = new FakeDevice();
		dev->setUniqueId(QString("fake ") + QString::number(i));
		result[dev->getUniqueId()] = dev;
	}

	return result;
}


// executing command 
bool FakeDevice::executeCommand(CLDeviceCommand* command)
{
	return true;
}

CLStreamreader* FakeDevice::getDeviceStreamConnection()
{
	return new FakeStreamReader(this);
}