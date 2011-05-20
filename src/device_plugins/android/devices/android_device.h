#ifndef android_h_2054_h
#define android_h_2054_h
#include "device/network_device.h"



class CLANdroidDevice : public CLNetworkDevice
{
public:
    CLANdroidDevice ();
    ~CLANdroidDevice ();

    DeviceType getDeviceType() const;

    QString toString() const;

    CLStreamreader* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();

};



#endif //avigilon_h_16_43_h