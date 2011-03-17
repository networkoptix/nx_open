#include "recorder_device.h"

CLRecorderDevice::CLRecorderDevice()
{

}

CLRecorderDevice::~CLRecorderDevice()
{

}

CLDevice::DeviceType CLRecorderDevice::getDeviceType() const
{
	return RECORDER;
}

CLStreamreader* CLRecorderDevice::getDeviceStreamConnection()
{
	return 0;
}

bool CLRecorderDevice::unknownDevice() const
{
	return false;
}

CLNetworkDevice* CLRecorderDevice::updateDevice()
{
	return 0;
}
