#include "network/nettools.h"
#include "network/mdns.h"
#include "base/sleep.h"
#include "../onvif/dataproviders/sp_mjpeg.h"
#include "dlink_device.h"


CLDlinkDevice::CLDlinkDevice()
{
    setAuth("admin", "");
}

CLDlinkDevice::~CLDlinkDevice()
{

}


CLDevice::DeviceType CLDlinkDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLDlinkDevice::toString() const
{
    return QLatin1String("live dlink") + getName() + QLatin1Char(' ') + getUniqueId();
}

CLStreamreader* CLDlinkDevice::getDeviceStreamConnection()
{
    QString name = getName();

    return new MJPEGtreamreader(this, "ipcam/stream.cgi?nowprofileid=2&audiostream=0");
}

bool CLDlinkDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLDlinkDevice::updateDevice()
{
    return 0;
}
