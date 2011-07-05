#include "android_resource.h"
#include "../dataprovider/android_stream_reader.h"


CLANdroidDevice::CLANdroidDevice()
{
    setAfterRouter(true); // wifi, huli
}

CLANdroidDevice::~CLANdroidDevice()
{

}


QnResource::DeviceType CLANdroidDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLANdroidDevice::toString() const
{
    return QString("live android ") + getUniqueId();
}

QnStreamDataProvider* CLANdroidDevice::getDeviceStreamConnection()
{
    return new CLAndroidStreamreader(this);
}

bool CLANdroidDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLANdroidDevice::updateDevice()
{
    return 0;
}
