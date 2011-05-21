#include "iqeye_device.h"
#include "../streamreader/iqeye_stream_reader.h"


CLIQEyeDevice::CLIQEyeDevice()
{
    
}

CLIQEyeDevice::~CLIQEyeDevice()
{

}


CLDevice::DeviceType CLIQEyeDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLIQEyeDevice::toString() const
{
    return QString("live iqeye ") + getUniqueId();
}

CLStreamreader* CLIQEyeDevice::getDeviceStreamConnection()
{
    return new CLIQEyeStreamreader(this);
}

bool CLIQEyeDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLIQEyeDevice::updateDevice()
{
    return 0;
}
