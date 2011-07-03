#ifndef iqeye_h_2054_h
#define iqeye_h_2054_h

#include "resource/network_resource.h"


class CLIQEyeDevice : public CLNetworkDevice
{
public:
    CLIQEyeDevice();
    ~CLIQEyeDevice();

    DeviceType getDeviceType() const;

    QString toString() const;

    QnStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();

};



#endif //avigilon_h_16_43_h