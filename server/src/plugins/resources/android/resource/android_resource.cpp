#include "android_resource.h"
#include "../dataprovider/android_stream_reader.h"


CLANdroidDevice::CLANdroidDevice()
{
    setAfterRouter(true); // wifi, huli
}

CLANdroidDevice::~CLANdroidDevice()
{

}


CLDevice::DeviceType CLANdroidDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLANdroidDevice::toString() const
{
    return QString("live android ") + getUniqueId();
}

CLStreamreader* CLANdroidDevice::getDeviceStreamConnection()
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
