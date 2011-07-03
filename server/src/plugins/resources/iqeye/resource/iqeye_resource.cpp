#include "iqeye_resource.h"
#include "../dataprovider/iqeye_sp_h264.h"
#include "../dataprovider/iqeye_sp_mjpeg.h"

CLIQEyeDevice::CLIQEyeDevice()
{
    
}

CLIQEyeDevice::~CLIQEyeDevice()
{

}


QnResource::DeviceType CLIQEyeDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLIQEyeDevice::toString() const
{
    return QString("live iqeye ") + getUniqueId();
}

QnStreamDataProvider* CLIQEyeDevice::getDeviceStreamConnection()
{
    if (getName()=="IQ042S")
        return new CLIQEyeH264treamreader(this);
    else
        return new CLIQEyeMJPEGtreamreader(this);
}

bool CLIQEyeDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLIQEyeDevice::updateDevice()
{
    return 0;
}
