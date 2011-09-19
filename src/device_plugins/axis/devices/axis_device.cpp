
#include "network/nettools.h"
#include "network/mdns.h"
#include "base/sleep.h"
#include "../../onvif/dataproviders/sp_mjpeg.h"
#include "../../onvif/dataproviders/sp_h264.h"
#include "axis_device.h"


CLAxisDevice::CLAxisDevice()
{
    setAuth("root", "1");
}

CLAxisDevice::~CLAxisDevice()
{

}


CLDevice::DeviceType CLAxisDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLAxisDevice::toString() const
{
    return QLatin1String("live axis ") + getName() + QLatin1Char(' ') + getUniqueId();
}

CLStreamreader* CLAxisDevice::getDeviceStreamConnection()
{
    //IQ732N   IQ732S     IQ832N   IQ832S   IQD30S   IQD31S  IQD32S  IQM30S  IQM31S  IQM32S
    QString name = getName();
    //return new RTPH264Streamreader(this);
    return new MJPEGtreamreader(this, "mjpg/video.mjpg");
}

bool CLAxisDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLAxisDevice::updateDevice()
{
    return 0;
}
