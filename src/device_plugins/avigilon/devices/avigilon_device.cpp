#include "avigilon_device.h"
#include "../streamreader/avigilon_client_pull.h"


CLAvigilonDevice::CLAvigilonDevice()
{

}

CLAvigilonDevice::~CLAvigilonDevice()
{

}


CLDevice::DeviceType CLAvigilonDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLAvigilonDevice::toString() const
{
    return QLatin1String("live clearpix ") + getUniqueId();
}

CLStreamreader* CLAvigilonDevice::getDeviceStreamConnection()
{
    return new CLAvigilonStreamreader(this);
}

bool CLAvigilonDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLAvigilonDevice::updateDevice()
{
    return 0;
}
