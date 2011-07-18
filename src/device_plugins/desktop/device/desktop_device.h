#ifndef __DESKTOP_DEVICE_H
#define __DESKTOP_DEVICE_H
#include "device/network_device.h"

class CLDesktopDevice: public CLDevice
{
public:
    CLDesktopDevice();
    ~CLDesktopDevice();

    DeviceType getDeviceType() const;

    QString toString() const;

    CLStreamreader* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();
};

#endif //__DESKTOP_DEVICE_H
