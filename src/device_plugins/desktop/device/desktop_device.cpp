#ifdef Q_OS_WIN

#include "desktop_device.h"
#include "device_plugins/desktop/streamreader/desktop_stream_reader.h"

CLDesktopDevice::CLDesktopDevice()
{

}

CLDesktopDevice::~CLDesktopDevice()
{

}

CLDevice::DeviceType CLDesktopDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLDesktopDevice::toString() const
{
    return getUniqueId();
}

CLStreamreader* CLDesktopDevice::getDeviceStreamConnection()
{
    return new CLDesktopStreamreader(this);
}

bool CLDesktopDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLDesktopDevice::updateDevice()
{
    return 0;
}

#endif // Q_OS_WIN
