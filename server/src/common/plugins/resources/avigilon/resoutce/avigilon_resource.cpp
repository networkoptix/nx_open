#include "avigilon_resource.h"
#include "../dataprovider/avigilon_client_pull.h"


CLAvigilonDevice::CLAvigilonDevice()
{
 
}

CLAvigilonDevice::~CLAvigilonDevice()
{

}


QnResource::DeviceType CLAvigilonDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLAvigilonDevice::toString() const
{
    return QString("live clearpix ") + getUniqueId();
}

QnStreamDataProvider* CLAvigilonDevice::getDeviceStreamConnection()
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
