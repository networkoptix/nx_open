#ifndef avigilon_h_16_43_h
#define avigilon_h_16_43_h

#include "common/device/network_device.h"


class CLAvigilonDevice : public CLNetworkDevice
{
public:
    CLAvigilonDevice();
    ~CLAvigilonDevice();

    DeviceType getDeviceType() const;

    QString toString() const;

    CLStreamreader* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();

};



#endif //avigilon_h_16_43_h