#ifndef axis_h_2054_h
#define axis_h_2054_h
#include "device/network_device.h"

class CLAxisDevice : public CLNetworkDevice
{
public:
    CLAxisDevice ();
    ~CLAxisDevice();

    DeviceType getDeviceType() const;

    QString toString() const;

    CLStreamreader* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();


};

#endif //avigilon_h_16_43_h
