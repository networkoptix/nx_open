#ifndef iqeye_h_2054_h
#define iqeye_h_2054_h
#include "device/network_device.h"

class CLIQEyeDevice : public CLNetworkDevice
{
public:
    CLIQEyeDevice();
    ~CLIQEyeDevice();

    DeviceType getDeviceType() const;

    QString toString() const;

    CLStreamreader* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();

    static void findDevices(CLDeviceList& result);
};

#endif //avigilon_h_16_43_h
