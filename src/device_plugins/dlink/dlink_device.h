#ifndef dlink_h_2054_h
#define dlink_h_2054_h
#include "device/network_device.h"

class CLDlinkDevice : public CLNetworkDevice
{
public:
    CLDlinkDevice();
    ~CLDlinkDevice();

    DeviceType getDeviceType() const;

    QString toString() const;

    CLStreamreader* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();


};

#endif //dlink_h_2054_h
